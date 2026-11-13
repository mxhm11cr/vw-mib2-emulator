// Track A A4 — load the authentic converted de.vw.mib.MIBMain on JDK 21.
// Loads + links without initializing (initialize=false), mirroring the firmware's -noverify.
// Reports the dependency frontier: which referenced classes are still missing.
public class LoadMIBMain {
    public static void main(String[] args) throws Exception {
        String name = args.length > 0 ? args[0] : "de.vw.mib.MIBMain";
        ClassLoader cl = LoadMIBMain.class.getClassLoader();
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

            try {
                Class.forName(name, true, cl);
                java.lang.reflect.Method m = c.getDeclaredMethod("main", String[].class);
                System.out.println("invoking main() ...");
                m.invoke(null, (Object) new String[0]);
                System.out.println("main() returned");
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
}
