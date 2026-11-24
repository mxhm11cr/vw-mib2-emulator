import com.sun.net.httpserver.*;
import java.awt.image.BufferedImage;
import java.io.*;
import java.net.InetSocketAddress;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import javax.imageio.ImageIO;

public final class TrackAWebServer {
    private static HttpServer s_server;
    private static ClassLoader s_cl;
    private static volatile byte[] s_latestJpeg = null;
    private static volatile long s_lastFrameTime = 0;

    public static void start(ClassLoader cl, int port) {
        if (s_server != null) return;
        s_cl = cl;
        try {
            s_server = HttpServer.create(new InetSocketAddress("0.0.0.0", port), 0);
            s_server.createContext("/", new MainHandler());
            s_server.createContext("/api/stream", new StreamHandler());
            s_server.createContext("/api/screen.jpg", new ScreenHandler());
            s_server.createContext("/api/touch", new TouchHandler());
            s_server.createContext("/api/key", new KeyHandler());
            s_server.createContext("/api/hmi_event", new HmiEventHandler());
            s_server.createContext("/api/vehicle", new VehicleHandler());
            s_server.createContext("/api/doors", new DoorsHandler());
            s_server.createContext("/api/engine", new EngineHandler());
            s_server.createContext("/api/state", new StateHandler());
            s_server.setExecutor(Executors.newCachedThreadPool());
            s_server.start();

            // Background frame grabber to keep JPEG cache fresh
            Thread grabber = new Thread(() -> {
                File bmpFile = new File("frame_latest.bmp");
                while (true) {
                    try {
                        if (bmpFile.exists() && bmpFile.lastModified() > s_lastFrameTime) {
                            s_lastFrameTime = bmpFile.lastModified();
                            BufferedImage img = ImageIO.read(bmpFile);
                            if (img != null) {
                                ByteArrayOutputStream baos = new ByteArrayOutputStream(65536);
                                ImageIO.write(img, "jpg", baos);
                                s_latestJpeg = baos.toByteArray();
                            }
                        }
                        Thread.sleep(33); // ~30 fps
                    } catch (Throwable ignored) {
                        try { Thread.sleep(50); } catch (Throwable ignore) {}
                    }
                }
            }, "TrackA-FrameGrabber");
            grabber.setDaemon(true);
            grabber.start();

            System.out.println("========================================================================");
            System.out.println(" [TrackAWebServer] Volkswagen Golf 7 MIB2 Web Dashboard started!");
            System.out.println(" [TrackAWebServer] Live Web Interface: http://localhost:" + port + "/");
            System.out.println(" [TrackAWebServer] Interactive Touch, Telemetry, Doors & Audio Streaming Ready.");
            System.out.println("========================================================================");
        } catch (Throwable t) {
            System.out.println("[TrackAWebServer] Could not start web server on port " + port + ": " + t);
        }
    }

    private static Map<String, String> parseQuery(String query) {
        Map<String, String> map = new HashMap<>();
        if (query == null || query.isEmpty()) return map;
        for (String pair : query.split("&")) {
            int idx = pair.indexOf('=');
            if (idx > 0) {
                try {
                    String k = URLDecoder.decode(pair.substring(0, idx), StandardCharsets.UTF_8);
                    String v = URLDecoder.decode(pair.substring(idx + 1), StandardCharsets.UTF_8);
                    map.put(k, v);
                } catch (Exception ignored) {}
            }
        }
        return map;
    }

    private static void sendJson(HttpExchange ex, int code, String json) throws IOException {
        byte[] bytes = json.getBytes(StandardCharsets.UTF_8);
        ex.getResponseHeaders().set("Content-Type", "application/json; charset=utf-8");
        ex.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
        ex.sendResponseHeaders(code, bytes.length);
        try (OutputStream os = ex.getResponseBody()) {
            os.write(bytes);
        }
    }

    private static class MainHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange ex) throws IOException {
            String path = ex.getRequestURI().getPath();
            if (!path.equals("/") && !path.equals("/index.html")) {
                ex.sendResponseHeaders(404, -1);
                return;
            }
            byte[] html = DASHBOARD_HTML.getBytes(StandardCharsets.UTF_8);
            ex.getResponseHeaders().set("Content-Type", "text/html; charset=utf-8");
            ex.sendResponseHeaders(200, html.length);
            try (OutputStream os = ex.getResponseBody()) {
                os.write(html);
            }
        }
    }

    private static class StreamHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange ex) throws IOException {
            ex.getResponseHeaders().set("Content-Type", "multipart/x-mixed-replace; boundary=--frame");
            ex.getResponseHeaders().set("Cache-Control", "no-cache, private");
            ex.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
            ex.sendResponseHeaders(200, 0);
            OutputStream os = ex.getResponseBody();
            try {
                while (true) {
                    byte[] frame = s_latestJpeg;
                    if (frame != null && frame.length > 0) {
                        os.write(("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + frame.length + "\r\n\r\n").getBytes(StandardCharsets.UTF_8));
                        os.write(frame);
                        os.write("\r\n".getBytes(StandardCharsets.UTF_8));
                        os.flush();
                    }
                    Thread.sleep(40); // 25 fps
                }
            } catch (Throwable ignored) {
            } finally {
                try { os.close(); } catch (Throwable ignored) {}
            }
        }
    }

    private static class ScreenHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange ex) throws IOException {
            byte[] frame = s_latestJpeg;
            if (frame == null) {
                File bmpFile = new File("frame_latest.bmp");
                if (bmpFile.exists()) {
                    try {
                        BufferedImage img = ImageIO.read(bmpFile);
                        if (img != null) {
                            ByteArrayOutputStream baos = new ByteArrayOutputStream(65536);
                            ImageIO.write(img, "jpg", baos);
                            frame = baos.toByteArray();
                        }
                    } catch (Throwable ignored) {}
                }
            }
            if (frame != null) {
                ex.getResponseHeaders().set("Content-Type", "image/jpeg");
                ex.getResponseHeaders().set("Cache-Control", "no-store");
                ex.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
                ex.sendResponseHeaders(200, frame.length);
                try (OutputStream os = ex.getResponseBody()) {
                    os.write(frame);
                }
            } else {
                ex.sendResponseHeaders(503, -1);
            }
        }
    }

    private static class TouchHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange ex) throws IOException {
            Map<String, String> q = parseQuery(ex.getRequestURI().getRawQuery());
            try {
                int a = Integer.parseInt(q.getOrDefault("a", "1"));
                int x = Integer.parseInt(q.getOrDefault("x", "0"));
                int y = Integer.parseInt(q.getOrDefault("y", "0"));
                int gesture = 1; // CLICK
                if (a == 0) gesture = 4; // PRESS
                else if (a == 1) gesture = 5; // DRAG
                else if (a == 2) gesture = 3; // RELEASE
                else if (a == 3) gesture = 1; // CLICK
                LoadMIBMain.dispatchTouch(s_cl, gesture, x, y);
                sendJson(ex, 200, "{\"ok\":true}");
            } catch (Throwable t) {
                sendJson(ex, 400, "{\"ok\":false,\"error\":\"" + t.getMessage() + "\"}");
            }
        }
    }

    private static class KeyHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange ex) throws IOException {
            Map<String, String> q = parseQuery(ex.getRequestURI().getRawQuery());
            try {
                int code = Integer.parseInt(q.getOrDefault("code", "0"));
                LoadMIBMain.dispatchHardKey(s_cl, code, 0);
                sendJson(ex, 200, "{\"ok\":true}");
            } catch (Throwable t) {
                sendJson(ex, 400, "{\"ok\":false,\"error\":\"" + t.getMessage() + "\"}");
            }
        }
    }

    private static class HmiEventHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange ex) throws IOException {
            Map<String, String> q = parseQuery(ex.getRequestURI().getRawQuery());
            try {
                int id = Integer.parseInt(q.getOrDefault("id", "0"));
                LoadMIBMain.dispatchHMIEvent(s_cl, id);
                sendJson(ex, 200, "{\"ok\":true}");
            } catch (Throwable t) {
                sendJson(ex, 400, "{\"ok\":false,\"error\":\"" + t.getMessage() + "\"}");
            }
        }
    }

    private static class VehicleHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange ex) throws IOException {
            Map<String, String> q = parseQuery(ex.getRequestURI().getRawQuery());
            String key = q.get("key");
            String value = q.get("value");
            if (key != null && value != null) {
                try {
                    double v = Double.parseDouble(value);
                    switch (key) {
                        case "speed": TrackAVehicleSim.vehicleSpeed = v; break;
                        case "rpm": TrackAVehicleSim.vehicleRpm = v; break;
                        case "consumption": TrackAVehicleSim.consumption = v; break;
                        case "avgConsumption": TrackAVehicleSim.avgConsumption = v; break;
                        case "fuel": TrackAVehicleSim.fuel = (int) v; break;
                        case "range": TrackAVehicleSim.range = (int) v; break;
                        case "tripDistance": TrackAVehicleSim.tripDistance = v; break;
                        case "avgSpeed": TrackAVehicleSim.avgSpeed = (float) v; break;
                        case "tripMinutes": TrackAVehicleSim.tripMinutes = (int) v; break;
                        case "odometer": TrackAVehicleSim.odometer = v; break;
                    }
                    TrackAVehicleSim.triggerFullRefresh();
                    sendJson(ex, 200, "{\"ok\":true}");
                    return;
                } catch (Throwable t) {
                    sendJson(ex, 400, "{\"ok\":false,\"error\":\"" + t.getMessage() + "\"}");
                    return;
                }
            }
            sendJson(ex, 400, "{\"ok\":false,\"error\":\"missing key or value\"}");
        }
    }

    private static class DoorsHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange ex) throws IOException {
            Map<String, String> q = parseQuery(ex.getRequestURI().getRawQuery());
            if (q.containsKey("lock")) {
                TrackAVehicleSim.doorsLocked = Boolean.parseBoolean(q.get("lock"));
            }
            if (q.containsKey("fl")) TrackAVehicleSim.doorFrontLeftOpen = Boolean.parseBoolean(q.get("fl"));
            if (q.containsKey("fr")) TrackAVehicleSim.doorFrontRightOpen = Boolean.parseBoolean(q.get("fr"));
            if (q.containsKey("rl")) TrackAVehicleSim.doorRearLeftOpen = Boolean.parseBoolean(q.get("rl"));
            if (q.containsKey("rr")) TrackAVehicleSim.doorRearRightOpen = Boolean.parseBoolean(q.get("rr"));
            if (q.containsKey("hood")) TrackAVehicleSim.hoodOpen = Boolean.parseBoolean(q.get("hood"));
            if (q.containsKey("trunk")) TrackAVehicleSim.trunkOpen = Boolean.parseBoolean(q.get("trunk"));
            TrackAVehicleSim.triggerFullRefresh();
            sendJson(ex, 200, "{\"ok\":true}");
        }
    }

    private static class EngineHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange ex) throws IOException {
            Map<String, String> q = parseQuery(ex.getRequestURI().getRawQuery());
            if (q.containsKey("running")) {
                TrackAVehicleSim.engineRunning = Boolean.parseBoolean(q.get("running"));
            } else {
                TrackAVehicleSim.engineRunning = !TrackAVehicleSim.engineRunning;
            }
            TrackAVehicleSim.triggerFullRefresh();
            sendJson(ex, 200, "{\"ok\":true,\"running\":" + TrackAVehicleSim.engineRunning + "}");
        }
    }

    private static class StateHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange ex) throws IOException {
            int gear = 1;
            double spd = TrackAVehicleSim.vehicleSpeed;
            if (spd > 140) gear = 6;
            else if (spd > 105) gear = 5;
            else if (spd > 75) gear = 4;
            else if (spd > 45) gear = 3;
            else if (spd > 20) gear = 2;

            double boost = 1.0 + (TrackAVehicleSim.vehicleRpm > 2000 ? Math.min(1.4, (TrackAVehicleSim.vehicleRpm - 2000) / 3000.0 * 1.4) : 0.0);
            double power = Math.min(162.0, (TrackAVehicleSim.vehicleRpm / 6000.0) * 162.0);
            int torque = (int) Math.min(350, TrackAVehicleSim.vehicleRpm > 1500 ? (200 + (TrackAVehicleSim.vehicleRpm - 1500) / 10) : 100);

            String json = String.format(Locale.ROOT,
                "{\"engineRunning\":%b,\"doorsLocked\":%b," +
                "\"doorFL\":%b,\"doorFR\":%b,\"doorRL\":%b,\"doorRR\":%b,\"hood\":%b,\"trunk\":%b," +
                "\"bluetooth\":{\"connected\":true,\"device\":\"Demo phone\",\"address\":\"02:00:00:00:00:01\",\"battery\":100,\"signal\":5,\"type\":\"4G\"}," +
                "\"vehicle\":{\"speed\":%.1f,\"rpm\":%.0f,\"consumption\":%.1f,\"avgConsumption\":%.1f," +
                "\"fuel\":%d,\"range\":%d,\"tripDistance\":%.1f,\"avgSpeed\":%.1f,\"tripMinutes\":%d,\"odometer\":%.1f," +
                "\"gear\":%d,\"boost\":%.2f,\"power\":%.0f,\"torque\":%d,\"coolantTemp\":90,\"oilTemp\":%.0f,\"outsideTemp\":19.5}}",
                TrackAVehicleSim.engineRunning, TrackAVehicleSim.doorsLocked,
                TrackAVehicleSim.doorFrontLeftOpen, TrackAVehicleSim.doorFrontRightOpen,
                TrackAVehicleSim.doorRearLeftOpen, TrackAVehicleSim.doorRearRightOpen,
                TrackAVehicleSim.hoodOpen, TrackAVehicleSim.trunkOpen,
                TrackAVehicleSim.vehicleSpeed, TrackAVehicleSim.vehicleRpm,
                TrackAVehicleSim.consumption, TrackAVehicleSim.avgConsumption,
                TrackAVehicleSim.fuel, TrackAVehicleSim.range,
                TrackAVehicleSim.tripDistance, TrackAVehicleSim.avgSpeed,
                TrackAVehicleSim.tripMinutes, TrackAVehicleSim.odometer,
                gear, boost, power, torque,
                TrackAVehicleSim.engineRunning ? (96.0 + spd / 25.0) : 22.0
            );
            sendJson(ex, 200, json);
        }
    }

    private static final String DASHBOARD_HTML =
"<!doctype html>\n" +
"<html lang=\"en\">\n" +
"<head>\n" +
"<meta charset=\"utf-8\">\n" +
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n" +
"<title>Volkswagen Golf 7 MIB2 Infotainment Control Center</title>\n" +
"<style>\n" +
"  :root { --bg:#0b0d11; --panel:#15181e; --panel-border:#252a34; --text:#e2e5eb; --dim:#828a96; --accent:#0088ff; --accent-glow:#0088ff44; --vw-blue:#001e50; --green:#2ecc71; --red:#e74c3c; }\n" +
"  * { box-sizing:border-box; }\n" +
"  body { margin:0; background:var(--bg); color:var(--text); font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif; font-size:13px; line-height:1.4; }\n" +
"  header { background:linear-gradient(180deg, #181d26 0%, #101318 100%); border-bottom:1px solid var(--panel-border); padding:10px 20px; display:flex; align-items:center; justify-content:space-between; }\n" +
"  header h1 { margin:0; font-size:16px; font-weight:600; letter-spacing:0.5px; display:flex; align-items:center; gap:10px; }\n" +
"  header .badge { background:#0055ff; color:#fff; font-size:10px; font-weight:700; padding:2px 8px; border-radius:12px; text-transform:uppercase; }\n" +
"  main { display:grid; grid-template-columns:820px 1fr; gap:16px; padding:16px; max-width:1600px; margin:auto; }\n" +
"  @media (max-width:1300px) { main { grid-template-columns:1fr; } }\n" +
"  .screen-card { background:var(--panel); border:1px solid var(--panel-border); border-radius:10px; padding:10px; box-shadow:0 8px 24px rgba(0,0,0,0.5); }\n" +
"  #screen { width:800px; height:480px; background:#000; border-radius:6px; display:block; touch-action:none; cursor:crosshair; user-select:none; }\n" +
"  .card { background:var(--panel); border:1px solid var(--panel-border); border-radius:8px; padding:12px; margin-bottom:12px; }\n" +
"  .card h2 { font-size:12px; text-transform:uppercase; letter-spacing:0.08em; color:var(--dim); margin:0 0 10px; display:flex; align-items:center; justify-content:space-between; }\n" +
"  .btn-grid { display:grid; grid-template-columns:repeat(auto-fill, minmax(88px, 1fr)); gap:6px; }\n" +
"  button { background:#202530; color:var(--text); border:1px solid var(--panel-border); border-radius:6px; padding:7px 10px; font-size:12px; font-weight:500; cursor:pointer; transition:all 0.15s; }\n" +
"  button:hover { background:#2c3444; border-color:#3d485e; }\n" +
"  button:active { transform:scale(0.97); }\n" +
"  button.active { background:var(--accent); border-color:var(--accent); color:#fff; box-shadow:0 0 10px var(--accent-glow); }\n" +
"  button.success { background:#27ae60; border-color:#27ae60; color:#fff; }\n" +
"  button.danger { background:#c0392b; border-color:#c0392b; color:#fff; }\n" +
"  .slider-row { display:grid; grid-template-columns:130px 1fr 65px; align-items:center; gap:10px; margin:5px 0; color:var(--dim); font-size:12px; }\n" +
"  input[type=range] { width:100%; accent-color:var(--accent); cursor:pointer; }\n" +
"  .val-badge { background:#0e1116; border:1px solid var(--panel-border); border-radius:4px; padding:2px 6px; text-align:right; font-variant-numeric:tabular-nums; color:var(--text); font-weight:600; }\n" +
"  .car-chassis { display:grid; grid-template-columns:1fr 1fr; gap:8px; background:#0e1116; border:1px solid var(--panel-border); border-radius:8px; padding:12px; }\n" +
"  .door-box { border:1px dashed #3a4252; border-radius:6px; padding:8px; text-align:center; }\n" +
"  .door-box.open { border-color:var(--red); background:rgba(231,76,60,0.1); color:var(--red); }\n" +
"  .audio-bar { display:flex; flex-direction:column; gap:8px; }\n" +
"  input[type=text] { width:100%; background:#0e1116; border:1px solid var(--panel-border); border-radius:6px; padding:8px 10px; color:var(--text); font-size:12px; outline:none; }\n" +
"  input[type=text]:focus { border-color:var(--accent); }\n" +
"  audio { width:100%; height:32px; border-radius:6px; margin-top:4px; }\n" +
"</style>\n" +
"</head>\n" +
"<body>\n" +
"<header>\n" +
"  <h1>Volkswagen Golf 7 MIB2 <span class=\"badge\">Authentic Emulator</span></h1>\n" +
"  <div id=\"headerStatus\" style=\"color:var(--dim); font-size:12px;\">Engine: <span id=\"engineStatus\" style=\"color:var(--green); font-weight:700;\">RUNNING</span> | Resolution: 800x480</div>\n" +
"</header>\n" +
"<main>\n" +
"  <div>\n" +
"    <div class=\"screen-card\">\n" +
"      <img id=\"screen\" alt=\"Volkswagen MIB2 800x480 Framebuffer Stream\" src=\"/api/stream\">\n" +
"    </div>\n" +
"    <div class=\"card\" style=\"margin-top:12px;\">\n" +
"      <h2>Authentic Hard Keys & Navigation Controls</h2>\n" +
"      <div class=\"btn-grid\" style=\"grid-template-columns:repeat(8, 1fr); margin-bottom:8px;\">\n" +
"        <button onclick=\"key(320)\">MENU</button>\n" +
"        <button onclick=\"key(282)\">RADIO</button>\n" +
"        <button onclick=\"key(326)\">MEDIA</button>\n" +
"        <button onclick=\"key(341)\">PHONE</button>\n" +
"        <button onclick=\"key(361)\">NAV</button>\n" +
"        <button onclick=\"key(263)\">TRAFFIC</button>\n" +
"        <button onclick=\"key(303)\" class=\"active\">CAR</button>\n" +
"        <button onclick=\"key(277)\">SETUP</button>\n" +
"      </div>\n" +
"      <div class=\"btn-grid\" style=\"grid-template-columns:repeat(5, 1fr);\">\n" +
"        <button onclick=\"hmiEv(1234000437)\">◀ PREV VIEW</button>\n" +
"        <button onclick=\"hmiEv(1234000436)\">▶ NEXT VIEW</button>\n" +
"        <button onclick=\"hmiEv(1234000393)\">⚙ CAR SETUP</button>\n" +
"        <button onclick=\"hmiEv(1234001184)\">☰ AUSWAHL</button>\n" +
"        <button onclick=\"key(286)\">⊙ ROTARY ENTER</button>\n" +
"      </div>\n" +
"    </div>\n" +
"    <div class=\"card\">\n" +
"      <h2>Bluetooth & iPhone 16 Pro Integration</h2>\n" +
"      <div style=\"background:#0e1116; border:1px solid var(--panel-border); border-radius:6px; padding:10px; display:flex; align-items:center; justify-content:space-between;\">\n" +
"        <div>\n" +
"          <div style=\"font-weight:700; color:#fff; font-size:13px; display:flex; align-items:center; gap:8px;\">\n" +
"            <span style=\"color:#0088ff;\">&#9742;</span> Demo phone\n" +
"            <span class=\"badge\" style=\"background:#27ae60; font-size:9px;\">CONNECTED</span>\n" +
"          </div>\n" +
"          <div style=\"color:var(--dim); font-size:11px; margin-top:3px;\">\n" +
"            MAC: 02:00:00:00:00:01 | Network: 4G (5 Bars) | Battery: 100% | Profiles: HFP, A2DP, AVRCP, PBAP\n" +
"          </div>\n" +
"        </div>\n" +
"        <div style=\"display:flex; gap:6px;\">\n" +
"          <button onclick=\"key(341)\">PHONE</button>\n" +
"          <button onclick=\"key(326)\">BT-AUDIO</button>\n" +
"        </div>\n" +
"      </div>\n" +
"    </div>\n" +
"    <div class=\"card\">\n" +
"      <h2>Virtual Vehicle Engine & Ignition State</h2>\n" +
"      <div style=\"display:flex; gap:10px;\">\n" +
"        <button id=\"btnEngine\" onclick=\"toggleEngine()\" class=\"success\" style=\"flex:1; padding:10px;\">ENGINE: RUNNING (Clamp 15 ON)</button>\n" +
"        <button id=\"btnLock\" onclick=\"toggleLock()\" style=\"flex:1; padding:10px;\">CENTRAL LOCKING: UNLOCKED</button>\n" +
"      </div>\n" +
"    </div>\n" +
"  </div>\n" +
"  <div>\n" +
"    <div class=\"card\">\n" +
"      <h2>Live Golf 7 Performance Telemetry</h2>\n" +
"      <div style=\"display:grid; grid-template-columns:repeat(4, 1fr); gap:8px; text-align:center;\">\n" +
"        <div style=\"background:#0e1116; border:1px solid var(--panel-border); border-radius:6px; padding:8px;\">\n" +
"          <div style=\"color:var(--dim); font-size:10px;\">GEAR</div>\n" +
"          <div id=\"val_gear\" style=\"font-size:18px; font-weight:700; color:var(--accent);\">D1</div>\n" +
"        </div>\n" +
"        <div style=\"background:#0e1116; border:1px solid var(--panel-border); border-radius:6px; padding:8px;\">\n" +
"          <div style=\"color:var(--dim); font-size:10px;\">BOOST</div>\n" +
"          <div id=\"val_boost\" style=\"font-size:18px; font-weight:700; color:#e67e22;\">1.00 bar</div>\n" +
"        </div>\n" +
"        <div style=\"background:#0e1116; border:1px solid var(--panel-border); border-radius:6px; padding:8px;\">\n" +
"          <div style=\"color:var(--dim); font-size:10px;\">COOLANT</div>\n" +
"          <div id=\"val_coolant\" style=\"font-size:18px; font-weight:700; color:#2ecc71;\">90 &deg;C</div>\n" +
"        </div>\n" +
"        <div style=\"background:#0e1116; border:1px solid var(--panel-border); border-radius:6px; padding:8px;\">\n" +
"          <div style=\"color:var(--dim); font-size:10px;\">OIL TEMP</div>\n" +
"          <div id=\"val_oil\" style=\"font-size:18px; font-weight:700; color:#2ecc71;\">98 &deg;C</div>\n" +
"        </div>\n" +
"      </div>\n" +
"    </div>\n" +
"    <div class=\"card\">\n" +
"      <h2>Trip Computer & Telemetry Sliders (Golf 7)</h2>\n" +
"      <div id=\"telemetrySliders\"></div>\n" +
"    </div>\n" +
"    <div class=\"card\">\n" +
"      <h2>Doors & Openings Simulation (MFD Warning 43008)</h2>\n" +
"      <div class=\"car-chassis\">\n" +
"        <div class=\"door-box\" id=\"box_fl\">\n" +
"          <b>Front Left (Driver)</b><br>\n" +
"          <button onclick=\"toggleDoor('fl')\" id=\"btn_fl\" style=\"margin-top:6px;\">CLOSED</button>\n" +
"        </div>\n" +
"        <div class=\"door-box\" id=\"box_fr\">\n" +
"          <b>Front Right</b><br>\n" +
"          <button onclick=\"toggleDoor('fr')\" id=\"btn_fr\" style=\"margin-top:6px;\">CLOSED</button>\n" +
"        </div>\n" +
"        <div class=\"door-box\" id=\"box_rl\">\n" +
"          <b>Rear Left</b><br>\n" +
"          <button onclick=\"toggleDoor('rl')\" id=\"btn_rl\" style=\"margin-top:6px;\">CLOSED</button>\n" +
"        </div>\n" +
"        <div class=\"door-box\" id=\"box_rr\">\n" +
"          <b>Rear Right</b><br>\n" +
"          <button onclick=\"toggleDoor('rr')\" id=\"btn_rr\" style=\"margin-top:6px;\">CLOSED</button>\n" +
"        </div>\n" +
"        <div class=\"door-box\" id=\"box_hood\">\n" +
"          <b>Engine Bonnet (Hood)</b><br>\n" +
"          <button onclick=\"toggleDoor('hood')\" id=\"btn_hood\" style=\"margin-top:6px;\">CLOSED</button>\n" +
"        </div>\n" +
"        <div class=\"door-box\" id=\"box_trunk\">\n" +
"          <b>Boot Lid (Trunk)</b><br>\n" +
"          <button onclick=\"toggleDoor('trunk')\" id=\"btn_trunk\" style=\"margin-top:6px;\">CLOSED</button>\n" +
"        </div>\n" +
"      </div>\n" +
"    </div>\n" +
"    <div class=\"card\">\n" +
"      <h2>Live Radio & Audio Stream Player</h2>\n" +
"      <div class=\"audio-bar\">\n" +
"        <div style=\"display:flex; gap:6px;\">\n" +
"          <input type=\"text\" id=\"audioUrl\" placeholder=\"Paste direct radio stream URL (MP3/AAC)...\" value=\"https://stream.live.vc.bbcmedia.co.uk/bbc_radio_one\">\n" +
"          <button onclick=\"playAudio()\" class=\"active\">PLAY</button>\n" +
"          <button onclick=\"stopAudio()\">STOP</button>\n" +
"        </div>\n" +
"        <div class=\"btn-grid\" style=\"grid-template-columns:repeat(4, 1fr); margin-top:4px;\">\n" +
"          <button onclick=\"setPreset('BBC Radio 1', 'https://stream.live.vc.bbcmedia.co.uk/bbc_radio_one')\">BBC Radio 1</button>\n" +
"          <button onclick=\"setPreset('Rock Antenne', 'https://stream.rockantenne.de/rockantenne/stream/mp3')\">Rock Antenne</button>\n" +
"          <button onclick=\"setPreset('1LIVE', 'https://wdr-1live-live.icecastssl.wdr.de/wdr/1live/live/mp3/128/stream.mp3')\">1LIVE</button>\n" +
"          <button onclick=\"setPreset('Swiss Pop', 'https://stream.srg-ssr.ch/m/radioswisspop/mp3_128')\">Swiss Pop</button>\n" +
"        </div>\n" +
"        <audio id=\"player\" controls></audio>\n" +
"      </div>\n" +
"    </div>\n" +
"  </div>\n" +
"</main>\n" +
"<script>\n" +
"  const img = document.getElementById('screen');\n" +
"  const post = (u) => fetch(u, { method:'POST' }).then(r => r.json().catch(() => ({})));\n" +
"  let down = false, lastMove = 0;\n" +
"  const touch = (a, e) => {\n" +
"    const r = img.getBoundingClientRect();\n" +
"    const x = Math.round((e.clientX - r.left) * 800 / r.width);\n" +
"    const y = Math.round((e.clientY - r.top) * 480 / r.height);\n" +
"    fetch(`/api/touch?a=${a}&x=${x}&y=${y}`, { method:'POST' });\n" +
"  };\n" +
"  img.addEventListener('pointerdown', e => { down = true; img.setPointerCapture(e.pointerId); touch(0, e); });\n" +
"  img.addEventListener('pointermove', e => { if (down && Date.now() - lastMove > 35) { lastMove = Date.now(); touch(1, e); } });\n" +
"  img.addEventListener('pointerup', e => { if (down) { down = false; touch(2, e); touch(3, e); } });\n" +
"  img.addEventListener('dragstart', e => e.preventDefault());\n" +
"\n" +
"  function key(code) { post('/api/key?code=' + code); }\n" +
"  function hmiEv(id) { post('/api/hmi_event?id=' + id); }\n" +
"\n" +
"  const SLIDERS = [\n" +
"    ['speed', 'Speed (km/h)', 0, 260, 1],\n" +
"    ['rpm', 'Engine (RPM)', 0, 7000, 50],\n" +
"    ['consumption', 'Consumption (l/100km)', 0, 30, 0.1],\n" +
"    ['avgConsumption', 'Avg Consumption', 0, 30, 0.1],\n" +
"    ['fuel', 'Fuel Tank (liters)', 0, 66, 1],\n" +
"    ['range', 'Estimated Range (km)', 0, 1200, 5],\n" +
"    ['tripDistance', 'Trip Distance (km)', 0, 2000, 0.1],\n" +
"    ['avgSpeed', 'Avg Speed (km/h)', 0, 200, 1],\n" +
"    ['tripMinutes', 'Trip Time (minutes)', 0, 1440, 1],\n" +
"    ['odometer', 'Total Odometer (km)', 0, 999999, 1]\n" +
"  ];\n" +
"  const tDiv = document.getElementById('telemetrySliders');\n" +
"  for (const [k, name, min, max, step] of SLIDERS) {\n" +
"    const row = document.createElement('div'); row.className = 'slider-row';\n" +
"    row.innerHTML = `<span>${name}</span><input type=\"range\" min=\"${min}\" max=\"${max}\" step=\"${step}\" id=\"sl_${k}\"><span class=\"val-badge\" id=\"vb_${k}\">0</span>`;\n" +
"    tDiv.append(row);\n" +
"    const sl = row.querySelector('input');\n" +
"    const slTimers = {};\n" +
"    sl.oninput = () => {\n" +
"      document.getElementById('vb_' + k).textContent = sl.value;\n" +
"      clearTimeout(slTimers[k]);\n" +
"      slTimers[k] = setTimeout(() => { post(`/api/vehicle?key=${k}&value=${sl.value}`); }, 50);\n" +
"    };\n" +
"    sl.onchange = () => { post(`/api/vehicle?key=${k}&value=${sl.value}`); };\n" +
"  }\n" +
"\n" +
"  let sState = {};\n" +
"  async function refresh() {\n" +
"    const s = await fetch('/api/state').then(r => r.json()).catch(() => null);\n" +
"    if (!s) return;\n" +
"    sState = s;\n" +
"    document.getElementById('engineStatus').textContent = s.engineRunning ? 'RUNNING' : 'STOPPED';\n" +
"    document.getElementById('engineStatus').style.color = s.engineRunning ? 'var(--green)' : 'var(--red)';\n" +
"    document.getElementById('btnEngine').textContent = s.engineRunning ? 'ENGINE: RUNNING (Clamp 15 ON)' : 'ENGINE: STOPPED (Clamp 15 OFF)';\n" +
"    document.getElementById('btnEngine').className = s.engineRunning ? 'success' : 'danger';\n" +
"    document.getElementById('btnLock').textContent = s.doorsLocked ? 'CENTRAL LOCKING: LOCKED' : 'CENTRAL LOCKING: UNLOCKED';\n" +
"    document.getElementById('btnLock').className = s.doorsLocked ? 'danger' : '';\n" +
"    if (s.vehicle) {\n" +
"      const g = s.vehicle.gear || 1;\n" +
"      document.getElementById('val_gear').textContent = s.engineRunning ? ('D' + g) : 'P';\n" +
"      document.getElementById('val_boost').textContent = (s.vehicle.boost || 1.0).toFixed(2) + ' bar';\n" +
"      document.getElementById('val_coolant').textContent = (s.vehicle.coolantTemp || 90) + ' \u00B0C';\n" +
"      document.getElementById('val_oil').textContent = Math.round(s.vehicle.oilTemp || 98) + ' \u00B0C';\n" +
"    }\n" +
"    ['fl','fr','rl','rr','hood','trunk'].forEach(d => {\n" +
"      const prop = d === 'fl' ? s.doorFL : d === 'fr' ? s.doorFR : d === 'rl' ? s.doorRL : d === 'rr' ? s.doorRR : d === 'hood' ? s.hood : s.trunk;\n" +
"      const box = document.getElementById('box_' + d);\n" +
"      const btn = document.getElementById('btn_' + d);\n" +
"      box.classList.toggle('open', !!prop);\n" +
"      btn.textContent = prop ? 'OPEN' : 'CLOSED';\n" +
"      btn.className = prop ? 'danger' : '';\n" +
"    });\n" +
"    for (const [k, v] of Object.entries(s.vehicle)) {\n" +
"      const sl = document.getElementById('sl_' + k);\n" +
"      if (sl && document.activeElement !== sl) {\n" +
"        sl.value = v;\n" +
"        document.getElementById('vb_' + k).textContent = v;\n" +
"      }\n" +
"    }\n" +
"  }\n" +
"  setInterval(refresh, 500); refresh();\n" +
"\n" +
"  function toggleEngine() { post('/api/engine').then(refresh); }\n" +
"  function toggleLock() { post('/api/doors?lock=' + !sState.doorsLocked).then(refresh); }\n" +
"  function toggleDoor(d) {\n" +
"    const cur = d === 'fl' ? sState.doorFL : d === 'fr' ? sState.doorFR : d === 'rl' ? sState.doorRL : d === 'rr' ? sState.doorRR : d === 'hood' ? sState.hood : sState.trunk;\n" +
"    post(`/api/doors?${d}=` + !cur).then(refresh);\n" +
"  }\n" +
"  const p = document.getElementById('player');\n" +
"  function playAudio() {\n" +
"    const u = document.getElementById('audioUrl').value.trim();\n" +
"    if (u) { p.src = u; p.play(); }\n" +
"  }\n" +
"  function stopAudio() { p.pause(); p.src = ''; }\n" +
"  function setPreset(name, url) {\n" +
"    document.getElementById('audioUrl').value = url;\n" +
"    playAudio();\n" +
"  }\n" +
"</script>\n" +
"</body>\n" +
"</html>";
}
