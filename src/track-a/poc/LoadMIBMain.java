import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.List;
import java.util.Set;

// Track A A4 — load the authentic converted de.vw.mib.MIBMain on JDK 21.
// Loads + links without initializing (initialize=false), mirroring the firmware's -noverify.
// Reports the dependency frontier: which referenced classes are still missing.
public class LoadMIBMain {
    public static volatile ClassLoader s_cl = null;
    public static volatile boolean s_initialScreenShown = false;

    public static native int pollInputEvent(int[] out);
    public static native void requestScreenClear();

    static {
        try {
            System.loadLibrary("tsd.mibstd2.hmi.dsi.native");
        } catch (Throwable ignored) {}
        try {
            System.loadLibrary("tsd.opengl.jsr231.native");
        } catch (Throwable ignored) {}
    }

    public static void nativeSendHardKey(int pressed, int released) {
        if (s_cl != null) {
            dispatchHardKey(s_cl, pressed, released);
        }
    }

    public static void nativeSendTouch(int gesture, int x, int y) {
        if (s_cl != null) {
            dispatchTouch(s_cl, gesture, x, y);
        }
    }

    public static void nativeSendRotary(int steps) {
        if (s_cl != null) {
            int pressed = steps > 0 ? 288 : 283;
            dispatchHardKey(s_cl, pressed, 0);
        }
    }

    public static void main(String[] args) throws Exception {
        String name = args.length > 0 ? args[0] : "de.vw.mib.MIBMain";
        ClassLoader cl = LoadMIBMain.class.getClassLoader();
        s_cl = cl;

        // Dedicated asynchronous input poller: bridges Win32 GUI message pump events directly to ASL without JNI thread blocking
        Thread inputPoller = new Thread(() -> {
            int[] event = new int[4];
            while (true) {
                try {
                    while (pollInputEvent(event) > 0) {
                        int type = event[0];
                        int p1 = event[1];
                        int p2 = event[2];
                        int p3 = event[3];
                        if (type == 1) { // HardKey: p1=pressed, p3=released
                            System.out.println("[WindowInputPoller] Dispatched HK event: " + p1 + " released=" + p3);
                            dispatchHardKey(s_cl, p1, p3);
                        } else if (type == 2) { // Touch: p1=gesture, p2=x, p3=y
                            dispatchTouch(s_cl, p1, p2, p3);
                        } else if (type == 3) { // Rotary: p1=steps
                            int pressed = p1 > 0 ? 288 : 283;
                            System.out.println("[WindowInputPoller] Dispatched Rotary step: " + p1 + " -> " + pressed);
                            dispatchHardKey(s_cl, pressed, 0);
                        }
                    }
                    Thread.sleep(16);
                } catch (Throwable ignored) {}
            }
        }, "WindowInputPoller");
        inputPoller.setDaemon(true);
        inputPoller.start();
        Class<?> c = Class.forName(name, false, cl);
        System.out.println("LOADED: " + c.getName());
        System.out.println("super: " + c.getSuperclass().getName());
        System.out.println("interfaces: " + java.util.Arrays.toString(c.getInterfaces()));
        System.out.println("modifiers: 0x" + Integer.toHexString(c.getModifiers()));
        try {
            System.out.println("declaredFields: " + c.getDeclaredFields().length);
        } catch (Throwable t) {
            System.out.println("declaredFields FAILED: " + t);
        }
        try {
            System.out.println("declaredMethods: " + c.getDeclaredMethods().length);
        } catch (Throwable t) {
            System.out.println("declaredMethods FAILED: " + t);
        }
        try {
            java.lang.reflect.Method m = c.getDeclaredMethod("main", String[].class);
            System.out.println("main method resolved: " + m);
        } catch (Throwable t) {
            System.out.println("main resolve FAILED: " + t);
        }
        if (args.length > 1 && args[1].equals("init")) {
            try {
                Class.forName(name, true, cl);
                System.out.println("INITIALIZED: <clinit> ran without error");
            } catch (Throwable t) {
                System.out.println("INIT frontier (first missing service): " + t);
                Throwable cause = t.getCause();
                int depth = 0;
                while (cause != null && depth++ < 6) {
                    System.out.println("  caused by: " + cause);
                    cause = cause.getCause();
                }
            }
        }
        if (args.length > 1 && args[1].equals("run")) {
            // Immediate fast watcher for NavStartupTarget (1330026), LSC (5150) and AslTargetStartup (5900)
            new Thread(() -> {
                boolean navReadySent = false;
                for (int i = 0; i < 300; i++) {
                    try {
                        Thread.sleep(50);

                        // Ensure Arabic font override is disabled so authentic Latin fonts & icons link properly
                        try {
                            Class<?> smCls = Class.forName("de.vw.mib.asl.framework.internal.framework.ServiceManager", false, cl);
                            Field cmField = smCls.getField("configManagerDiag");
                            Object cm = cmField.get(null);
                            if (cm != null) {
                                for (Class<?> k = cm.getClass(); k != null; k = k.getSuperclass()) {
                                    try {
                                        Field aField = k.getDeclaredField("arabicAvailable");
                                        aField.setAccessible(true);
                                        if (aField.getBoolean(cm)) {
                                            aField.setBoolean(cm, false);
                                            System.out.println("[FastNavStartupWatcher] Set ConfigurationManager.arabicAvailable = false");
                                        }
                                        break;
                                    } catch (NoSuchFieldException ignored) {}
                                }
                            }
                        } catch (Throwable ignored) {}

                        // Ensure Arabic font replacement is completely disabled in CachingFontManager
                        try {
                            Class<?> fontSm = Class.forName("de.vw.mib.graphics.font.internal.ServiceManager", false, cl);
                            Object cfgMgr = fontSm.getField("configurationManager").get(null);
                            if (cfgMgr != null) {
                                Field af = cfgMgr.getClass().getDeclaredField("arabicAvailable");
                                af.setAccessible(true);
                                af.setBoolean(cfgMgr, false);
                            }
                        } catch (Throwable ignored) {}

                        // Ensure aslNavStartupWasDoneAlready stays true throughout ASL startup
                        try {
                            Class<?> ssmCls = Class.forName("de.vw.mib.asl.framework.internal.startupv7r.StartupServiceManager", false, cl);
                            java.lang.reflect.Field f = ssmCls.getDeclaredField("aslNavStartupWasDoneAlready");
                            f.setAccessible(true);
                            if (!f.getBoolean(null)) {
                                f.setBoolean(null, true);
                            }
                        } catch (Throwable ignored) {}

                        Class<?> smCls = Class.forName("de.vw.mib.asl.framework.internal.framework.ServiceManager", false, cl);
                        Object ge = smCls.getField("eventMain").get(null);
                        if (ge != null) {
                            Class<?> geIfc = Class.forName("de.vw.mib.genericevents.GenericEvents", false, cl);
                            Class<?> edIfc = Class.forName("de.vw.mib.genericevents.EventDispatcherHSM", false, cl);
                            Class<?> egCls = Class.forName("de.vw.mib.genericevents.EventGeneric", false, cl);
                            Class<?> efIfc = Class.forName("de.vw.mib.genericevents.EventFactory", false, cl);

                            java.lang.reflect.Method getEd = geIfc.getMethod("getEventDispatcher");
                            getEd.setAccessible(true);
                            Object ed = getEd.invoke(ge);

                            java.lang.reflect.Method isTargetReg = edIfc.getMethod("isTargetRegistered", int.class);
                            isTargetReg.setAccessible(true);
                            boolean navReg = (Boolean) isTargetReg.invoke(ed, 1330026);

                            if (navReg && !navReadySent) {
                                Object kb = null;
                                Class<?> kbCls = null;
                                try {
                                    kbCls = Class.forName("de.vw.mib.asl.internal.navigation.startup.knowledge.KnowledgeBase", false, cl);
                                    kb = kbCls.getMethod("getInstance").invoke(null);
                                    if (kb != null) {
                                        Field gf = kbCls.getDeclaredField("groups");
                                        gf.setAccessible(true);
                                        Object[] groups = (Object[]) gf.get(kb);
                                        if (groups != null) {
                                            for (Object g : groups) {
                                                if (g != null) {
                                                    try {
                                                        Method stls = g.getClass().getMethod("setTrafficLightState", int.class);
                                                        stls.invoke(g, 3); // GREEN
                                                    } catch (Throwable ignored) {}
                                                }
                                            }
                                        }
                                        kbCls.getMethod("setNavStateOfOperation", int.class).invoke(kb, 5);
                                        kbCls.getMethod("setNavDomainState", int.class).invoke(kb, 2);
                                        kbCls.getMethod("setMainMapReady", boolean.class).invoke(kb, true);
                                        kbCls.getMethod("setMainMapDrawn", boolean.class).invoke(kb, true);
                                        kbCls.getMethod("notifyObserver").invoke(kb);
                                    }
                                } catch (Throwable ignored) {}

                                // Stop Nav fallback timer (100001) and notify ASL startup
                                try {
                                    Object navTarget = edIfc.getMethod("getTarget", int.class).invoke(ed, 1330026);
                                    if (navTarget != null) {
                                        try {
                                            Method st = navTarget.getClass().getMethod("stopTimer", int.class);
                                            st.setAccessible(true);
                                            st.invoke(navTarget, 100001);
                                        } catch (Throwable ignored) {}

                                        if (kb != null && kbCls != null) {
                                            try {
                                                Method mChanged = navTarget.getClass().getDeclaredMethod("changed", kbCls);
                                                mChanged.setAccessible(true);
                                                mChanged.invoke(navTarget, kb);
                                            } catch (Throwable ignored) {}
                                        }

                                        Field fNavDone = navTarget.getClass().getDeclaredField("navCompletelyReadySendedBackToAslStartup");
                                        fNavDone.setAccessible(true);
                                        if (!fNavDone.getBoolean(navTarget)) {
                                            Method m = navTarget.getClass().getDeclaredMethod("sendNavCompletelyReadyToAslStartup");
                                            m.setAccessible(true);
                                            m.invoke(navTarget);
                                            System.out.println("[FastNavStartupWatcher] Invoked sendNavCompletelyReadyToAslStartup() on NavStartupTarget!");
                                        }
                                    }
                                } catch (Throwable ignored) {}

                                java.lang.reflect.Method getEf = geIfc.getMethod("getEventFactory");
                                getEf.setAccessible(true);
                                Object ef = getEf.invoke(ge);

                                java.lang.reflect.Method newEv = efIfc.getMethod("newEvent", int.class, int.class, int.class);
                                newEv.setAccessible(true);

                                java.lang.reflect.Method sendSafe = edIfc.getMethod("sendSafe", egCls);
                                sendSafe.setAccessible(true);

                                // Notify LSC Bullhorn (5150) that Nav is ready -> immediate switch to Skin 1 & en_GB ONCE
                                Object evLsc = newEv.invoke(ef, 1330026, 5150, 40000);
                                egCls.getMethod("setInt", int.class, int.class).invoke(evLsc, 0, 1330041);
                                egCls.getMethod("setInt", int.class, int.class).invoke(evLsc, 1, 6);
                                egCls.getMethod("setString", int.class, String.class).invoke(evLsc, 2, "en_GB");
                                sendSafe.invoke(ed, evLsc);

                                // Notify AslTargetStartup (5900) EV_NAV_IS_COMPLETELY_READY ONCE
                                Object evAts = newEv.invoke(ef, 1330026, 5900, 100303);
                                sendSafe.invoke(ed, evAts);
                                navReadySent = true;
                                System.out.println("[FastNavStartupWatcher] Sent Nav complete events (LSC + Startup) once.");
                            }

                            // Advance any stalled startup work units (e.g. WORKUNIT_BAP__GENERIC)
                            if (i > 20) {
                                try {
                                    Class<?> ssmCls = Class.forName("de.vw.mib.asl.framework.internal.startupv7r.StartupServiceManager", false, cl);
                                    Object curWu = ssmCls.getField("currentActiveWorkUnit").get(null);
                                    if (curWu != null) {
                                        try {
                                            Method mNext = curWu.getClass().getMethod("doNextWorkStepAfterASLCallback");
                                            mNext.setAccessible(true);
                                            mNext.invoke(curWu);
                                        } catch (Throwable ignored) {}
                                        try {
                                            Method mDomain = curWu.getClass().getMethod("doNextWorkStepAfterDomainAnswer");
                                            mDomain.setAccessible(true);
                                            mDomain.invoke(curWu);
                                        } catch (Throwable ignored) {}
                                    }
                                } catch (Throwable ignored) {}
                            }

                            // Check if startup is already done
                            try {
                                Class<?> ssmCls = Class.forName("de.vw.mib.asl.framework.internal.startupv7r.StartupServiceManager", false, cl);
                                Field sdF = ssmCls.getDeclaredField("startupDone");
                                sdF.setAccessible(true);
                                if (sdF.getBoolean(null)) {
                                    System.out.println("[FastNavStartupWatcher] Authentic ASL Startup fully completed (startupDone=true) in " + (i * 50) + "ms!");
                                    break;
                                }
                            } catch (Throwable ignored) {}
                        }
                    } catch (Throwable t) {
                        if (i % 40 == 0) System.out.println("[FastNavStartupWatcher] check notice: " + t);
                    }
                }
            }, "FastNavStartupWatcher").start();

            // Start virtual vehicle simulation for authentic Golf 7 CAR menus
            try {
                Class<?> vsCls = Class.forName("TrackAVehicleSim", false, cl);
                vsCls.getMethod("start", ClassLoader.class).invoke(null, cl);
            } catch (Throwable t) {
                System.out.println("[LoadMIBMain] Could not start TrackAVehicleSim: " + t);
            }

            // Start interactive HTTP Web Server on port 8080 (live stream, touch, telemetry, doors, audio)
            try {
                Class<?> wsCls = Class.forName("TrackAWebServer", false, cl);
                wsCls.getMethod("start", ClassLoader.class, int.class).invoke(null, cl, 8080);
            } catch (Throwable t) {
                System.out.println("[LoadMIBMain] Could not start TrackAWebServer: " + t);
            }

            new Thread(() -> {
                try {
                    Thread.sleep(8000);
                    Class<?> osgiCls = Class.forName("de.vw.mib.osgi.MIBOsgi", false, cl);
                    Object osgi = osgiCls.getMethod("getInstance").invoke(null);
                    java.lang.reflect.Method getBundleState = osgiCls.getMethod("getBundleState", int.class);
                    System.out.println("=== OSGi BUNDLE STATUS (8s) ===");
                    int active = 0, total = 0;
                    for (int id = 1; id <= 76; id++) {
                        int state = (Integer) getBundleState.invoke(osgi, id);
                        if (state != 0) {
                            total++;
                            String stateStr = state == 32 ? "ACTIVE" : state == 8 ? "STARTING" : state == 4 ? "RESOLVED" : state == 2 ? "INSTALLED" : ("STATE_" + state);
                            if (state == 32) active++;
                            System.out.println("  Bundle " + id + ": [" + stateStr + "]");
                        }
                    }
                    System.out.println("=== ACTIVE BUNDLES: " + active + "/" + total + " ===");
                    System.out.println("=== REGISTERED DSI EVENTS ===");
                    try {
                        Class<?> dispCls = Class.forName("tsd.mibstd2.hmi.dsi.communication.DSIRXDispatcherThread", false, cl);
                        Object disp = dispCls.getMethod("getInstance").invoke(null);
                        java.lang.reflect.Field emField = dispCls.getDeclaredField("eventMap");
                        emField.setAccessible(true);
                        Object eventMap = emField.get(disp);
                        System.out.println("eventMap: " + eventMap);
                    } catch (Throwable t) {
                        System.out.println("DSI events error: " + t);
                    }
                    System.out.println("=== END REGISTERED DSI EVENTS ===");
                    System.out.println("=== STARTUP STATE ===");
                    try {
                        Class<?> ssm = Class.forName("de.vw.mib.asl.framework.internal.startupv7r.StartupServiceManager", false, cl);
                        Object curWu = ssm.getField("currentActiveWorkUnit").get(null);
                        System.out.println("currentActiveWorkUnit: " + curWu);
                        if (curWu != null) {
                            java.lang.reflect.Method getName = curWu.getClass().getMethod("getName");
                            java.lang.reflect.Method getStep = curWu.getClass().getMethod("getWorkstepForDebugging");
                            System.out.println("  wu name: " + getName.invoke(curWu));
                            System.out.println("  wu step: " + getStep.invoke(curWu));
                        }
                        System.out.println("startUpCalled: " + ssm.getField("startUpCalled").get(null));
                        java.lang.reflect.Field sdF = ssm.getDeclaredField("startupDone");
                        sdF.setAccessible(true);
                        System.out.println("startupDone: " + sdF.get(null));
                        System.out.println("displayWasAlreadySetReady: " + ssm.getField("displayWasAlreadySetReady").get(null));
                    } catch (Throwable t) {
                        System.out.println("StartupServiceManager diag error: " + t);
                    }
                    try {
                        Class<?> atsCls = Class.forName("de.vw.mib.asl.framework.internal.startupv7r.AslTargetStartup", false, cl);
                        Object atsInst = atsCls.getField("aslTargetStartupInstance").get(null);
                        System.out.println("AslTargetStartup instance: " + atsInst);
                        if (atsInst != null) {
                            java.lang.reflect.Field dlsF = atsCls.getDeclaredField("domainLoadStatus");
                            dlsF.setAccessible(true);
                            System.out.println("  domainLoadStatus: " + dlsF.get(null));
                            java.lang.reflect.Field lrdF = atsCls.getDeclaredField("lastRequestedDomains");
                            lrdF.setAccessible(true);
                            System.out.println("  lastRequestedDomains: " + lrdF.get(atsInst));

                            // In emulator environment without live GPS hardware, signal EV_NAV_IS_COMPLETELY_READY (100303)
                            // to avoid the 60-second fallback timer in SpecialMethodWaitForNavToFinishToContinueStartupStd
                            Class<?> ssmCls = Class.forName("de.vw.mib.asl.framework.internal.startupv7r.StartupServiceManager", false, cl);
                            boolean navDone = ssmCls.getField("aslNavStartupWasDoneAlready").getBoolean(null);
                            if (!navDone) {
                                System.out.println("[LoadMIBMain] Triggering EV_NAV_IS_COMPLETELY_READY (100303) on AslTargetStartup");
                                java.lang.reflect.Method triggerMeSafe = atsCls.getMethod("triggerMeSafe", int.class);
                                triggerMeSafe.invoke(atsInst, 100303);
                            }
                        }
                    } catch (Throwable t) {
                        System.out.println("AslTargetStartup diag error: " + t);
                    }
                    try {
                        Class<?> smCls = Class.forName("de.vw.mib.startup.internal.StartupManager", false, cl);
                        Class<?> actCls = Class.forName("de.vw.mib.startup.internal.Activator", false, cl);
                        Class<?> brCls = Class.forName("de.vw.mib.osgi.internal.BundleRegistry", false, cl);
                        Object br = brCls.getMethod("getInstance").invoke(null);
                        Object b15 = brCls.getMethod("getBundle", int.class).invoke(br, 15);
                        Object b15Act = null;
                        if (b15 != null) {
                            java.lang.reflect.Field actF = b15.getClass().getDeclaredField("bundleActivator");
                            actF.setAccessible(true);
                            b15Act = actF.get(b15);
                        }
                        if (b15Act != null) {
                            java.lang.reflect.Field smF = actCls.getDeclaredField("startupManager");
                            smF.setAccessible(true);
                            Object sm = smF.get(b15Act);
                            if (sm != null) {
                                java.lang.reflect.Field skF = smCls.getDeclaredField("skinReady"); skF.setAccessible(true);
                                java.lang.reflect.Field lumF = smCls.getDeclaredField("lumReady"); lumF.setAccessible(true);
                                java.lang.reflect.Field bndF = smCls.getDeclaredField("bundlesStarted"); bndF.setAccessible(true);
                                java.lang.reflect.Field dspF = smCls.getDeclaredField("dispatcherStarted"); dspF.setAccessible(true);
                                System.out.println("StartupManager: skinReady=" + skF.get(sm) + ", lumReady=" + lumF.get(sm) +
                                    ", bundlesStarted=" + bndF.get(sm) + ", dispatcherStarted=" + dspF.get(sm));
                            }
                        }
                    } catch (Throwable t) {
                        System.out.println("StartupManager diag error: " + t);
                    }
                    System.out.println("=== END STARTUP STATE ===");
                    System.out.println("=== RUNNING THREADS ===");
                    for (java.util.Map.Entry<Thread, StackTraceElement[]> e : Thread.getAllStackTraces().entrySet()) {
                        Thread t = e.getKey();
                        if (t.isAlive()) {
                            System.out.println("Thread [" + t.getName() + "] state=" + t.getState());
                            for (StackTraceElement ste : e.getValue()) {
                                System.out.println("    at " + ste);
                            }
                        }
                    }
                    System.out.println("=== END THREADS ===");
                    Thread.sleep(14000);
                    System.out.println("=== OSGi BUNDLE STATUS (22s) ===");
                    active = 0; total = 0;
                    for (int id = 1; id <= 76; id++) {
                        int state = (Integer) getBundleState.invoke(osgi, id);
                        if (state != 0) {
                            total++;
                            String stateStr = state == 32 ? "ACTIVE" : state == 8 ? "STARTING" : state == 4 ? "RESOLVED" : state == 2 ? "INSTALLED" : ("STATE_" + state);
                            if (state == 32) active++;
                        }
                    }
                    System.out.println("=== ACTIVE BUNDLES (22s): " + active + "/" + total + " ===");
                    try {
                        Class<?> ssm = Class.forName("de.vw.mib.asl.framework.internal.startupv7r.StartupServiceManager", false, cl);
                        java.lang.reflect.Field sdF = ssm.getDeclaredField("startupDone");
                        sdF.setAccessible(true);
                        System.out.println("=== STARTUP STATUS (22s) ===");
                        System.out.println("  startupDone: " + sdF.get(null));
                        Object curWu = ssm.getField("currentActiveWorkUnit").get(null);
                        System.out.println("  currentActiveWorkUnit: " + curWu);
                        if (curWu != null) {
                            java.lang.reflect.Method getName = curWu.getClass().getMethod("getName");
                            java.lang.reflect.Method getStep = curWu.getClass().getMethod("getWorkstepForDebugging");
                            System.out.println("  wu name: " + getName.invoke(curWu));
                            System.out.println("  wu step: " + getStep.invoke(curWu));
                            try {
                                java.lang.reflect.Method getModel = curWu.getClass().getMethod("getModel");
                                Object model = getModel.invoke(curWu);
                                java.lang.reflect.Method getSm = model.getClass().getMethod("getSpecialMethod");
                                Object sm = getSm.invoke(model);
                                System.out.println("  wu special method: " + (sm != null ? sm.getClass().getName() : "null"));
                            } catch (Throwable t) {}
                        }
                        System.out.println("=============================");
                    } catch (Throwable t) {
                        System.out.println("StartupServiceManager 22s diag error: " + t);
                    }
                    System.out.println("=== RUNNING THREADS (22s) ===");
                    for (java.util.Map.Entry<Thread, StackTraceElement[]> e : Thread.getAllStackTraces().entrySet()) {
                        Thread t = e.getKey();
                        if (t.isAlive()) {
                            System.out.println("Thread [" + t.getName() + "] state=" + t.getState());
                        }
                    }
                    System.out.println("=== END THREADS (22s) ===");
                } catch (Throwable t) {
                    System.out.println("Bundle status monitor error: " + t);
                }
            }, "OSGi-Status-Monitor").start();

            // Auto-switch to initial screen (RADIO) as soon as startup completes, eliminating black screen
            new Thread(() -> {
                for (int i = 0; i < 30; i++) {
                    try {
                        Thread.sleep(500);
                        if (s_initialScreenShown) break;
                        Class<?> ssm = Class.forName("de.vw.mib.asl.framework.internal.startupv7r.StartupServiceManager", false, cl);
                        java.lang.reflect.Field sdF = ssm.getDeclaredField("startupDone");
                        sdF.setAccessible(true);
                        if (Boolean.TRUE.equals(sdF.get(null))) {
                            s_initialScreenShown = true;
                            System.out.println("[LoadMIBMain] System startup complete -> Displaying initial screen (RADIO)...");
                            dispatchHardKey(cl, 282, 281);
                            break;
                        }
                    } catch (Throwable ignored) {}
                }
                if (!s_initialScreenShown) {
                    s_initialScreenShown = true;
                    System.out.println("[LoadMIBMain] Initial boot timeout reached -> Displaying initial screen (RADIO)...");
                    dispatchHardKey(cl, 282, 281);
                }
            }, "AutoInitialScreen").start();

            // Console stdin reader for interactive Golf 7 keyboard input
            new Thread(() -> {
                try {
                    java.io.InputStream in = System.in;
                    int ch;
                    while ((ch = in.read()) != -1) {
                        handleConsoleKey(cl, (char) ch);
                    }
                } catch (Throwable ignored) {}
            }, "ConsoleKeyReader").start();

            try {
                Class.forName(name, true, cl);
                java.lang.reflect.Method m = c.getDeclaredMethod("main", String[].class);
                System.out.println("invoking main() ...");
                m.invoke(null, (Object) new String[0]);
                System.out.println("main() returned");
                System.out.println("========================================================================");
                System.out.println(" [VW-MIB2] Authentic Volkswagen MIB2 runtime active.");
                System.out.println(" [VW-MIB2] Interactive Golf 7 controls enabled:");
                System.out.println("   [C]         CAR menu (Driving data, vehicle status, lights, settings)");
                System.out.println("   [R]         RADIO (FM presets, tuning ruler, station list)");
                System.out.println("   [E]         MEDIA (USB, SD card, music library)");
                System.out.println("   [N]         NAVIGATION (Maps, destination search, guidance)");
                System.out.println("   [M] / [Home] MENU (Authentic Golf 7 icon grid menu)");
                System.out.println("   [S]         SETUP (System settings, display, sound)");
                System.out.println("   [P]         PHONE (Telephony, Bluetooth, contacts)");
                System.out.println("   [<-] / [->] PREV / NEXT station or track");
                System.out.println("   [^] / [v]   Rotary knob turn (tuning, list scroll)");
                System.out.println("   [Enter]     Rotary knob push (select / enter)");
                System.out.println("   [Mouse]     Touchscreen click, drag, and interact");
                System.out.println("========================================================================");
                // Keep the main thread alive indefinitely so the JVM does not exit
                while (true) {
                    try {
                        Thread.sleep(60000);
                    } catch (InterruptedException ie) {
                        break;
                    }
                }
            } catch (Throwable t) {
                System.out.println("RUN frontier (first missing service): " + t);
                Throwable cause = t.getCause();
                int depth = 0;
                while (cause != null && depth++ < 8) {
                    System.out.println("  caused by: " + cause);
                    cause = cause.getCause();
                }
            }
        }
    }

    public static void dispatchDSIKey(ClassLoader cl, int code, boolean pressed) {
        try {
            Class<?> dispCls = Class.forName("tsd.mibstd2.hmi.dsi.communication.DSIRXDispatcherThread", false, cl);
            Class<?> baseCls = Class.forName("tsd.mibstd2.hmi.dsi.base.DSIBaseImpl", false, cl);
            Method mapIdToApp = dispCls.getDeclaredMethod("mapIdToApp", int.class);
            mapIdToApp.setAccessible(true);
            Object disp = dispCls.getMethod("getInstance").invoke(null);
            @SuppressWarnings("unchecked")
            List<?> apps = (List<?>) mapIdToApp.invoke(disp, 0x0201d800); // DSIKeyPanel
            if (apps != null && !apps.isEmpty()) {
                Method listenersByAttr = baseCls.getDeclaredMethod("getAllDSIListenersByAttribute", int.class);
                listenersByAttr.setAccessible(true);
                for (Object app : apps) {
                    @SuppressWarnings("unchecked")
                    List<Object> ls = (List<Object>) listenersByAttr.invoke(app, 25 /* ATTR_KEY2 */);
                    if (ls != null) {
                        for (Object l : ls) {
                            try {
                                ((org.dsi.ifc.keypanel.DSIKeyPanelListener) l).updateKey2(13 /* KEYBOARD_FRONT */, code, pressed ? 1 : 0, 0, 1);
                            } catch (Throwable ignored) {}
                        }
                    }
                }
            }
        } catch (Throwable t) {
            // Keep going
        }
    }

    public static void dispatchDSIGesture(ClassLoader cl, int g, int x, int y) {
        try {
            Class<?> dispCls = Class.forName("tsd.mibstd2.hmi.dsi.communication.DSIRXDispatcherThread", false, cl);
            Class<?> baseCls = Class.forName("tsd.mibstd2.hmi.dsi.base.DSIBaseImpl", false, cl);
            Method mapIdToApp = dispCls.getDeclaredMethod("mapIdToApp", int.class);
            mapIdToApp.setAccessible(true);
            Object disp = dispCls.getMethod("getInstance").invoke(null);
            @SuppressWarnings("unchecked")
            List<?> apps = (List<?>) mapIdToApp.invoke(disp, 0x0201d800); // DSIKeyPanel
            if (apps != null && !apps.isEmpty()) {
                Method listenersByAttr = baseCls.getDeclaredMethod("getAllDSIListenersByAttribute", int.class);
                listenersByAttr.setAccessible(true);
                int time = (int) System.currentTimeMillis();
                for (Object app : apps) {
                    @SuppressWarnings("unchecked")
                    List<Object> ls = (List<Object>) listenersByAttr.invoke(app, 20 /* ATTR_GESTURE2 */);
                    if (ls != null) {
                        for (Object l : ls) {
                            try {
                                ((org.dsi.ifc.keypanel.DSIKeyPanelListener) l).updateGesture2(13 /* GESTURE_COORDINATES */, g, 1, false, x, y, 0, 0, time, 1);
                            } catch (Throwable ignored) {}
                        }
                    }
                }
            }
        } catch (Throwable t) {
            // Keep going
        }
    }

    public static void dispatchDSIRotary(ClassLoader cl, int steps) {
        try {
            Class<?> dispCls = Class.forName("tsd.mibstd2.hmi.dsi.communication.DSIRXDispatcherThread", false, cl);
            Class<?> baseCls = Class.forName("tsd.mibstd2.hmi.dsi.base.DSIBaseImpl", false, cl);
            Method mapIdToApp = dispCls.getDeclaredMethod("mapIdToApp", int.class);
            mapIdToApp.setAccessible(true);
            Object disp = dispCls.getMethod("getInstance").invoke(null);
            @SuppressWarnings("unchecked")
            List<?> apps = (List<?>) mapIdToApp.invoke(disp, 0x0201d800);
            if (apps != null && !apps.isEmpty()) {
                Method listenersByAttr = baseCls.getDeclaredMethod("getAllDSIListenersByAttribute", int.class);
                listenersByAttr.setAccessible(true);
                for (Object app : apps) {
                    @SuppressWarnings("unchecked")
                    List<Object> ls = (List<Object>) listenersByAttr.invoke(app, 23 /* ATTR_ENCODER2 */);
                    if (ls != null) {
                        for (Object l : ls) {
                            try {
                                ((org.dsi.ifc.keypanel.DSIKeyPanelListener) l).updateEncoder2(13, 16 /* KEY_DDS */, steps, 0, 1);
                            } catch (Throwable ignored) {}
                        }
                    }
                }
            }
        } catch (Throwable t) {
            // Keep going
        }
    }

    public static void dispatchHardKey(ClassLoader cl, int pressed, int released) {
        if (pressed == 303 || pressed == 282 || pressed == 326 || pressed == 361 || pressed == 320 || pressed == 277 || pressed == 341 || pressed == 306 || pressed == 263 || pressed == 2110065 || pressed == 2110070) {
            try { requestScreenClear(); } catch (Throwable ignored) {}
        }

        // Dispatch to authentic Golf 7 DSIKeyPanel hardware keys
        int dsiKey = -1;
        switch (pressed) {
            case 303: dsiKey = 6; break;  // CAR
            case 282: dsiKey = 15; break; // RADIO
            case 326: dsiKey = 1; break;  // MEDIA
            case 361: dsiKey = 4; break;  // NAV
            case 320: dsiKey = 78; break; // MENU
            case 341: dsiKey = 3; break;  // PHONE
            case 277: dsiKey = 7; break;  // SETUP
            case 263: dsiKey = 5; break;  // TRAFFIC
            case 283: dsiKey = 10; break; // PREV
            case 288: dsiKey = 14; break; // NEXT
        }
        if (dsiKey != -1) {
            TrackAVehicleSim.triggerFullRefresh();
            dispatchDSIKey(cl, dsiKey, true);
            final int dk = dsiKey;
            new Thread(() -> {
                try { Thread.sleep(60); dispatchDSIKey(cl, dk, false); } catch (Throwable ignored) {}
            }).start();
        }

        try {
            Class<?> sm = Class.forName("de.vw.mib.asl.framework.internal.framework.ServiceManager", false, cl);
            Object disp = sm.getField("eventDispatcher").get(null);
            if (disp != null) {
                try {
                    java.lang.reflect.Field f = disp.getClass().getDeclaredField("lsChangeFilter");
                    f.setAccessible(true);
                    f.setBoolean(disp, false);
                } catch (Throwable ignored) {}

                Class<?> ifc = Class.forName("de.vw.mib.event.dispatcher.SystemEventDispatcher", false, cl);
                java.lang.reflect.Method m = ifc.getMethod("createAndSubmitHardkeyEvent", int.class);
                m.setAccessible(true);
                m.invoke(disp, pressed);
                System.out.println("[LoadMIBMain] Successfully dispatched HK event: " + pressed);
                if (released > 0) {
                    new Thread(() -> {
                        try {
                            Thread.sleep(60);
                            m.invoke(disp, released);
                            System.out.println("[LoadMIBMain] Successfully dispatched HK release event: " + released);
                        } catch (Throwable ignored) {}
                    }).start();
                }
            }
        } catch (Throwable t) {
            System.out.println("[LoadMIBMain] HardKey dispatch error: " + t);
        }
    }

    public static void dispatchHMIEvent(ClassLoader cl, int eventId) {
        try {
            Class<?> sm = Class.forName("de.vw.mib.asl.framework.internal.framework.ServiceManager", false, cl);
            Object disp = sm.getField("eventDispatcher").get(null);
            if (disp != null) {
                try {
                    java.lang.reflect.Field f = disp.getClass().getDeclaredField("lsChangeFilter");
                    f.setAccessible(true);
                    f.setBoolean(disp, false);
                } catch (Throwable ignored) {}

                Class<?> ifc = Class.forName("de.vw.mib.event.dispatcher.SystemEventDispatcher", false, cl);
                java.lang.reflect.Method m = ifc.getMethod("createAndSubmitHMIEvent", int.class);
                m.setAccessible(true);
                m.invoke(disp, eventId);
                System.out.println("[LoadMIBMain] Successfully dispatched authentic HMI event: " + eventId);
            }
        } catch (Throwable t) {
            System.out.println("[LoadMIBMain] HMI event dispatch error: " + t);
        }
    }

    public static void dispatchTouch(ClassLoader cl, int gesture, int x, int y) {
        // Dispatch directly through authentic Golf 7 DSIKeyPanel gesture2
        dispatchDSIGesture(cl, gesture, x, y);

        // Also dispatch through SystemEventDispatcher
        try {
            Class<?> sm = Class.forName("de.vw.mib.asl.framework.internal.framework.ServiceManager", false, cl);
            Object disp = sm.getField("eventDispatcher").get(null);
            if (disp != null) {
                try {
                    java.lang.reflect.Field f = disp.getClass().getDeclaredField("lsChangeFilter");
                    f.setAccessible(true);
                    f.setBoolean(disp, false);
                } catch (Throwable ignored) {}

                Class<?> ifc = Class.forName("de.vw.mib.event.dispatcher.SystemEventDispatcher", false, cl);
                if (gesture == 1) { // CLICK: (x, y, clickCount, gestureParam)
                    java.lang.reflect.Method m = ifc.getMethod("createAndSubmitTouchEventClick", int.class, int.class, int.class, int.class);
                    m.setAccessible(true);
                    m.invoke(disp, x, y, 1, -1);
                } else if (gesture == 4) { // PRESS: (x, y, param)
                    java.lang.reflect.Method m = ifc.getMethod("createAndSubmitTouchEventPress", int.class, int.class, int.class);
                    m.setAccessible(true);
                    m.invoke(disp, x, y, -1);
                } else if (gesture == 3) { // RELEASE: (x, y, param)
                    java.lang.reflect.Method m = ifc.getMethod("createAndSubmitTouchEventRelease", int.class, int.class, int.class);
                    m.setAccessible(true);
                    m.invoke(disp, x, y, -1);
                } else if (gesture == 5) { // DRAG: (x, y, dx, dy, param)
                    java.lang.reflect.Method m = ifc.getMethod("createAndSubmitTouchEventDrag", int.class, int.class, int.class, int.class, int.class);
                    m.setAccessible(true);
                    m.invoke(disp, x, y, 0, 0, -1);
                }
            }
        } catch (Throwable t) {
            System.out.println("[LoadMIBMain] Touch dispatch error: " + t);
        }
    }

    public static void sendTouchClick(ClassLoader cl, int x, int y) {
        System.out.println("[LoadMIBMain] sendTouchClick at (" + x + ", " + y + ")");
        dispatchTouch(cl, 4, x, y); // PRESS
        try { Thread.sleep(25); } catch (Throwable ignored) {}
        dispatchTouch(cl, 1, x, y); // CLICK
        try { Thread.sleep(25); } catch (Throwable ignored) {}
        dispatchTouch(cl, 3, x, y); // RELEASE
    }

    public static void sendTouchSwipe(ClassLoader cl, int x1, int y1, int x2, int y2) {
        System.out.println("[LoadMIBMain] sendTouchSwipe (" + x1 + "," + y1 + ") -> (" + x2 + "," + y2 + ")");
        dispatchTouch(cl, 4, x1, y1); // PRESS
        for (int i = 1; i <= 6; i++) {
            int cx = x1 + (x2 - x1) * i / 6;
            int cy = y1 + (y2 - y1) * i / 6;
            try { Thread.sleep(15); } catch (Throwable ignored) {}
            dispatchTouch(cl, 5, cx, cy); // DRAG
        }
        try { Thread.sleep(15); } catch (Throwable ignored) {}
        dispatchTouch(cl, 3, x2, y2); // RELEASE
    }

    public static void handleConsoleKey(ClassLoader cl, char ch) {
        if (ch >= '1' && ch <= '5') {
            int presetIdx = ch - '1';
            int[] presetX = { 120, 260, 400, 540, 680 };
            int x = presetX[presetIdx];
            System.out.println("[ConsoleKey] Triggering Golf 7 Preset " + ch);
            sendTouchClick(cl, x, 260); // Radio preset
            // Also click Driving Data metric tab if in Car menu
            if (ch == '1') sendTouchClick(cl, 250, 40); // Since start
            else if (ch == '2') sendTouchClick(cl, 400, 40); // Long-term
            else if (ch == '3') sendTouchClick(cl, 550, 40); // Since refueling
            return;
        }

        switch (ch) {
            case '<': case ',':
                System.out.println("[ConsoleKey] CAR Prev View: authentic HMI event 1234000437");
                dispatchHMIEvent(cl, 1234000437);
                return;
            case '>': case '.':
                System.out.println("[ConsoleKey] CAR Next View: authentic HMI event 1234000436");
                dispatchHMIEvent(cl, 1234000436);
                return;
            case 'w': case 'W':
                System.out.println("[ConsoleKey] CAR Auswahl/View: click (80, 440) & event 1234001184");
                sendTouchClick(cl, 80, 440);
                dispatchHMIEvent(cl, 1234001184);
                return;
            case 'u': case 'U':
                System.out.println("[ConsoleKey] CAR Setup/Settings: click (730, 440), DSI KEY_SETUP & event 1234000393");
                sendTouchClick(cl, 730, 440);
                dispatchDSIKey(cl, 7 /* KEY_SETUP */, true);
                dispatchHMIEvent(cl, 1234000393);
                new Thread(() -> { try { Thread.sleep(60); dispatchDSIKey(cl, 7, false); } catch (Throwable ignored) {} }).start();
                return;
            case 'z': case 'Z':
                System.out.println("[ConsoleKey] CAR swipe left (next view)");
                sendTouchSwipe(cl, 600, 240, 200, 240);
                return;
            case 'x': case 'X':
                System.out.println("[ConsoleKey] CAR swipe right (prev view)");
                sendTouchSwipe(cl, 200, 240, 600, 240);
                return;
        }

        int pressed = 0, released = 0;
        switch (Character.toUpperCase(ch)) {
            case 'C': pressed = 303; released = 304; break; // CAR
            case 'R': pressed = 282; released = 281; break; // RADIO
            case 'E': pressed = 326; released = 325; break; // MEDIA
            case 'N': pressed = 361; released = 360; break; // NAV
            case 'M': pressed = 320; released = 319; break; // MENU
            case 'H': pressed = 2110070; released = 2110071; break; // HOME
            case 'S': pressed = 277; released = 276; break; // SETUP
            case 'P': pressed = 341; released = 342; break; // PHONE
            case 'T': pressed = 263; released = 264; break; // TRAFFIC
            case 'A': pressed = 2110065; released = 2110066; break; // APP
            case 'V': pressed = 306; released = 269; break; // CLIMATE
            case '[': case 'K':
                System.out.println("[ConsoleKey] ROTARY KNOB LEFT (DDS -1)");
                dispatchDSIRotary(cl, -1);
                pressed = 283;
                break;
            case ']': case 'L':
                System.out.println("[ConsoleKey] ROTARY KNOB RIGHT (DDS +1)");
                dispatchDSIRotary(cl, 1);
                pressed = 288;
                break;
            case ' ': case 'O': pressed = 286; released = 287; break; // DDS ROTARY ENTER
        }
        if (pressed != 0) {
            System.out.println("[ConsoleKey] Triggering Golf 7 HK Event: pressed=" + pressed + " released=" + released);
            dispatchHardKey(cl, pressed, released);
        }
    }
}
