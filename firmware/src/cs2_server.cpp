#include "cs2_server.h"

#include <WiFi.h>
#include <WiFiUdp.h>

#include <vector>

#include "app.h"
#include "cs2_lan.h"
#include "event_log.h"
#include "layout_store.h"

namespace cs2server {

namespace {

constexpr int kMaxClients = 4;
constexpr int kMaxPeers = 4;
constexpr uint32_t kPeerTimeoutMs = 120000;

struct Client {
  WiFiClient sock;
  uint8_t buf[cs2::kFrameSize];
  uint8_t len = 0;
  bool active = false;
};

struct Peer {
  IPAddress ip;
  uint32_t lastMs = 0;
  bool used = false;
};

WiFiServer server(cs2::kTcpPort, kMaxClients);
WiFiUDP udp;
Client clients[kMaxClients];
Peer peers[kMaxPeers];
cs2::Gateway gateway;
bool started = false;
// Origen de la trama que se está reenviando (para no devolvérsela).
int sourceClient = -1;
int sourcePeer = -1;
uint32_t fromApps = 0;

// ---- Datos que se entregan a la app ------------------------------------------

const char* turnoutType(const std::string& article) {
  if (article == "24630") return "dreiwegweiche";
  // Referencias C: impar = izquierda (24611, 24671, 24711), par = derecha.
  if (!article.empty() && (article.back() - '0') % 2 == 1) return "linksweiche";
  return "rechtsweiche";
}

void addArticle(cs2::Model* m, uint32_t uid, const std::string& name, const char* typ) {
  marklin::Protocol proto;
  uint32_t address;
  if (!marklin::decodeAccessoryUid(uid, &proto, &address)) return;
  for (const auto& a : m->articles)
    if (a.id == address) return;
  m->articles.push_back({static_cast<uint16_t>(address), name, typ, proto == marklin::Protocol::DCC});
}

void fillModel(cs2::Model* m) {
  for (const auto& l : config.locos) m->locos.push_back({l.label.empty() ? l.name : l.label, l.uid, l.fn});
  if (m->locos.empty())
    for (int i = 0; i < controller.locoCount(); i++) {
      const uint32_t uid = controller.loco(i).uid;
      m->locos.push_back({locoLabel(uid).c_str(), uid, {}});
    }

  for (const auto& t : layouts::turnouts()) {
    const char* typ = turnoutType(t.article);
    if (t.uid2 && !(t.article == "24630" && t.uid2 == t.uid + 1)) {
      // Dos accesorios que no son consecutivos: dos desvíos simples.
      addArticle(m, t.uid, t.name, "linksweiche");
      addArticle(m, t.uid2, t.name + " 2", "rechtsweiche");
    } else {
      addArticle(m, t.uid, t.name, typ);
    }
  }

  for (const auto& s : config.sensors) m->contacts.push_back({s.id, s.name});
  for (const auto& s : layouts::sensors()) {
    bool dup = false;
    for (const auto& c : m->contacts) dup = dup || c.id == s.id;
    if (!dup) m->contacts.push_back({s.id, s.name});
  }
  if (!layouts::activeTitle().empty()) m->pageName = layouts::activeTitle();
}

// ---- Envío ---------------------------------------------------------------------

void writeFrames(WiFiClient& sock, const std::vector<marklin::CanFrame>& frames) {
  std::vector<uint8_t> out(frames.size() * cs2::kFrameSize);
  for (size_t i = 0; i < frames.size(); i++) cs2::encodeFrame(frames[i], &out[i * cs2::kFrameSize]);
  sock.write(out.data(), out.size());
}

void sendUdp(const IPAddress& ip, uint16_t port, const marklin::CanFrame& f) {
  uint8_t raw[cs2::kFrameSize];
  cs2::encodeFrame(f, raw);
  udp.beginPacket(ip, port);
  udp.write(raw, sizeof(raw));
  udp.endPacket();
}

void broadcast(const marklin::CanFrame& f) {
  if (!started) return;
  uint8_t raw[cs2::kFrameSize];
  cs2::encodeFrame(f, raw);
  for (int i = 0; i < kMaxClients; i++)
    if (clients[i].active && i != sourceClient) clients[i].sock.write(raw, sizeof(raw));
  const uint32_t now = millis();
  for (int i = 0; i < kMaxPeers; i++) {
    if (!peers[i].used || i == sourcePeer) continue;
    if (now - peers[i].lastMs > kPeerTimeoutMs) {
      peers[i].used = false;
      continue;
    }
    sendUdp(peers[i].ip, cs2::kUdpReplyPort, f);
  }
}

// ---- Recepción -----------------------------------------------------------------

void logRequest(const marklin::CanFrame& f) {
  char name[9];
  memcpy(name, f.data, 8);
  name[8] = 0;
  eventLog.addf(LogKind::Info, "CS2 network: app requests '%s'", name);
}

// Procesa una trama de una app; las respuestas propias van en `replies`.
void fromApp(const marklin::CanFrame& f, int client, int peer, std::vector<marklin::CanFrame>* replies) {
  fromApps++;
  const bool forward = gateway.handleClientFrame(f, fillModel, replies);
  if (marklin::commandOf(f) == marklin::CMD_CONFIG_DATA_REQUEST && !replies->empty()) logRequest(f);
  if (!forward) return;
  sourceClient = client;
  sourcePeer = peer;
  canBus.send(f);
  // Actualiza el estado (velocidades, desvíos...), lo registra en el
  // monitor y lo reenvía al resto de apps a través de onBusFrame.
  controller.handleFrame(f);
  sourceClient = -1;
  sourcePeer = -1;
}

void acceptClients() {
  WiFiClient c = server.available();
  if (!c) return;
  for (int i = 0; i < kMaxClients; i++) {
    if (clients[i].active) continue;
    clients[i].sock = c;
    clients[i].sock.setNoDelay(true);
    clients[i].len = 0;
    clients[i].active = true;
    eventLog.addf(LogKind::Info, "CS2 network: app connected from %s", c.remoteIP().toString().c_str());
    return;
  }
  eventLog.add(LogKind::Error, "CS2 network: too many apps connected (max. 4)");
  c.stop();
}

void pollClients() {
  for (int i = 0; i < kMaxClients; i++) {
    Client& cl = clients[i];
    if (!cl.active) continue;
    if (!cl.sock.connected()) {
      cl.sock.stop();
      cl.active = false;
      eventLog.add(LogKind::Info, "CS2 network: app disconnected");
      continue;
    }
    for (int n = 0; n < 64 && cl.sock.available() > 0; n++) {
      const int r = cl.sock.read(cl.buf + cl.len, cs2::kFrameSize - cl.len);
      if (r <= 0) break;
      cl.len += r;
      if (cl.len < cs2::kFrameSize) continue;
      cl.len = 0;
      marklin::CanFrame f;
      if (!cs2::decodeFrame(cl.buf, &f)) continue;
      std::vector<marklin::CanFrame> replies;
      fromApp(f, i, -1, &replies);
      if (!replies.empty()) writeFrames(cl.sock, replies);
    }
  }
}

int touchPeer(const IPAddress& ip) {
  const uint32_t now = millis();
  int freeSlot = -1, oldest = 0;
  for (int i = 0; i < kMaxPeers; i++) {
    if (peers[i].used && peers[i].ip == ip) {
      peers[i].lastMs = now;
      return i;
    }
    if (!peers[i].used && freeSlot < 0) freeSlot = i;
    if (peers[i].lastMs < peers[oldest].lastMs) oldest = i;
  }
  const int slot = freeSlot >= 0 ? freeSlot : oldest;
  peers[slot].ip = ip;
  peers[slot].lastMs = now;
  peers[slot].used = true;
  return slot;
}

void pollUdp() {
  for (int n = 0; n < 8; n++) {
    const int size = udp.parsePacket();
    if (size <= 0) return;
    const IPAddress ip = udp.remoteIP();
    const uint16_t port = udp.remotePort();
    const int peer = touchPeer(ip);
    uint8_t raw[cs2::kFrameSize];
    while (udp.read(raw, sizeof(raw)) == static_cast<int>(sizeof(raw))) {
      marklin::CanFrame f;
      if (!cs2::decodeFrame(raw, &f)) continue;
      std::vector<marklin::CanFrame> replies;
      fromApp(f, -1, peer, &replies);
      for (const auto& r : replies) {
        sendUdp(ip, cs2::kUdpReplyPort, r);
        if (port != cs2::kUdpReplyPort) sendUdp(ip, port, r);
      }
    }
  }
}

}  // namespace

void setup() {
  gateway.setUid(controller.uid());
  server.begin();
  server.setNoDelay(true);
  udp.begin(cs2::kUdpListenPort);
  started = true;
  eventLog.addf(LogKind::Info, "CS2 network: Central Station on TCP %u / UDP %u (RailControl Pro)",
                cs2::kTcpPort, cs2::kUdpListenPort);
}

void loop() {
  if (!started) return;
  acceptClients();
  pollClients();
  pollUdp();
}

void onBusFrame(const marklin::CanFrame& f, bool) { broadcast(f); }

void notifySensor(uint32_t id, bool active) { broadcast(cs2::s88Event(controller.hash(), id, active)); }

int tcpClients() {
  int n = 0;
  for (const auto& c : clients) n += c.active;
  return n;
}

int udpPeers() {
  int n = 0;
  const uint32_t now = millis();
  for (const auto& p : peers) n += p.used && now - p.lastMs <= kPeerTimeoutMs;
  return n;
}

bool configFile(const std::string& request, std::string* content) {
  cs2::Model model;
  fillModel(&model);
  return cs2::configFile(request, model, content);
}

uint32_t framesFromApps() { return fromApps; }
uint32_t configRequests() { return gateway.configRequests(); }

}  // namespace cs2server
