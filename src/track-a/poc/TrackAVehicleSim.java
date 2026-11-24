import java.lang.reflect.*;
import java.util.*;
import java.util.regex.Pattern;

/**
 * Track A Virtual Vehicle Simulation Engine for authentic Golf 7 MIB2 Infotainment.
 * Automatically discovers subscribed vehicle DSI services, enables authentic CAR menus,
 * submenus, driving data (Since Start, Long-term, Since Refueling), vehicle status,
 * lights, comfort, driving assistance, mirrors, ParkPilot, service info, phone, and media.
 */
public final class TrackAVehicleSim {
    private static final int VALID = 1;
    private static final int NORMAL_OPERATION = 2;
    private static final Pattern NOT_ON_ICE = Pattern.compile(
        "(?i)zeroemission|recover|hybrid|electric|emobility|plugin|charg|battery|secondaryengine|hv|fuelcell|cng|lpg|astamfa"
    );
    private static final Pattern FLAG_CLASSES = Pattern.compile(".*(Config|Configuration|Support|Elements|Features).*");

    private static final int[] dynamicEvents = {
        0x0200f800, // DSICarKombi (speed, odometer, fuel, trip computer)
        0x0201a800, // DSIGeneralVehicleStates (speed, engine, lights)
        0x0200d800, // DSICarEco (convenience consumers)
        0x02040800, // DSIPowerManagement (clamp 15 / ignition)
        0x0200a800, // DSICarComfort (doors, locks, windows)
        0x02029800, // DSIMediaPlayer (playback state, play position)
        0x02094800  // DSIMobileEquipment (phone network, activation state)
    };
    public static volatile boolean s_forceRefresh = false;
    private static int s_bootLoops = 0;

    public static void triggerFullRefresh() {
        s_forceRefresh = true;
    }

    // Dynamic Virtual Vehicle Telemetry State (Golf 7)
    public static volatile double vehicleSpeed = 0.0; // Parked with engine idling
    public static volatile double vehicleRpm = 820.0;
    public static volatile double consumption = 6.4;
    public static volatile double avgConsumption = 6.2;
    public static volatile int range = 640;
    public static volatile int fuel = 45;
    public static volatile double tripDistance = 18.4;
    public static volatile float avgSpeed = 58.0f;
    public static volatile int tripMinutes = 23;
    public static volatile double odometer = 18240.0;
    public static volatile boolean engineRunning = true;
    public static volatile boolean doorsLocked = false;
    public static volatile boolean doorFrontLeftOpen = false;
    public static volatile boolean doorFrontRightOpen = false;
    public static volatile boolean doorRearLeftOpen = false;
    public static volatile boolean doorRearRightOpen = false;
    public static volatile boolean hoodOpen = false;
    public static volatile boolean trunkOpen = false;

    private static volatile boolean running = false;
    private static Method mapIdToApp;
    private static Method listenersByAttr;

    public static void start(ClassLoader cl) {
        if (running) return;
        running = true;

        new Thread(() -> {
            try {
                // Wait for DSI dispatcher and services to register
                Thread.sleep(1500);
                Class<?> dispCls = Class.forName("tsd.mibstd2.hmi.dsi.communication.DSIRXDispatcherThread", false, cl);
                Class<?> baseCls = Class.forName("tsd.mibstd2.hmi.dsi.base.DSIBaseImpl", false, cl);
                mapIdToApp = dispCls.getDeclaredMethod("mapIdToApp", int.class);
                mapIdToApp.setAccessible(true);
                listenersByAttr = baseCls.getDeclaredMethod("getAllDSIListenersByAttribute", int.class);
                listenersByAttr.setAccessible(true);

                System.out.println("[VehicleSim] Virtual vehicle engine started for Golf 7 CAR menu");

                // Verified authentic dsi2hmi event IDs for Golf 7 vehicle services
                int[] carEvents = {
                    0x02002800, // DSIAudioManagement (connection state, fade-in)
                    0x02003800, // DSISound
                    0x02005800, // DSIBluetooth
                    0x02008800, // DSICarAirCondition (climate control)
                    0x02009800, // DSICarAuxHeaterCooler
                    0x0200a800, // DSICarComfort (mirrors, wipers, locking, windows)
                    0x0200b800, // DSICarDriverAssistance (ESC, ACC, Front Assist, Lane Assist)
                    0x0200c800, // DSICarDrivingCharacteristics (driving profile: Eco, Normal, Sport)
                    0x0200d800, // DSICarEco (eco tips, convenience consumers)
                    0x0200f800, // DSICarKombi (trip computer, driving data, odometer, fuel, SIA)
                    0x02010800, // DSICarLight (exterior, interior, coming/leaving home, ambient light)
                    0x02011800, // DSICarParkingSystem (ParkPilot, 360 OPS, sensors)
                    0x02012800, // DSICarSeat (seat adjustments)
                    0x02013800, // DSICarTimeUnitsLanguage (units, clock, language)
                    0x02014800, // DSICarVehicleStates (vehicle status, warnings, tire display)
                    0x02018800, // DSIDisplayManagement
                    0x02019800, // DSIFileBrowser
                    0x0201a800, // DSIGeneralVehicleStates (speed, engine, lights, ignition)
                    0x0201d800, // DSIKeyPanel (hard keys, rotary knob, touch gestures)
                    0x02020800, // DSIMapViewerControl (map ready, zoom list, map mode)
                    0x02027800, // DSIMediaBase (device info, media list)
                    0x02028800, // DSIMediaBrowser
                    0x02029800, // DSIMediaPlayer (playback state)
                    0x02031800, // DSINavigation (nav state of operation)
                    0x02038800, // DSIAdbSetup (phonebook / contacts setup)
                    0x0203b800, // DSIPersistence
                    0x02040800, // DSIPowerManagement (clamp 15 / ignition)
                    0x02041800, // DSIAMFMTuner
                    0x02048800, // DSIStartup
                    0x02094800, // DSIMobileEquipment (phone network, activation state, friendly name)
                    0x02095800, // DSIMobileEquipmentTopology (phone topology, usage)
                    0x02099800  // DSISmartphoneIntegration
                };

                while (running) {
                    s_bootLoops++;
                    initCarData(cl);
                    initNavigationData(cl);
                    initPhoneData(cl);
                    initMediaData(cl);

                    // Dynamic realistic vehicle physics
                    if (engineRunning) {
                        if (vehicleSpeed > 0.0) {
                            tripDistance += (vehicleSpeed / 3600.0);
                            odometer += (vehicleSpeed / 3600.0);
                            // Authentic Golf 7 gear-based RPM calculation
                            if (vehicleSpeed < 20.0) {
                                vehicleRpm = 1000.0 + (vehicleSpeed / 20.0) * 1800.0;
                            } else if (vehicleSpeed < 45.0) {
                                vehicleRpm = 1400.0 + ((vehicleSpeed - 20.0) / 25.0) * 1600.0;
                            } else if (vehicleSpeed < 75.0) {
                                vehicleRpm = 1600.0 + ((vehicleSpeed - 45.0) / 30.0) * 1500.0;
                            } else if (vehicleSpeed < 105.0) {
                                vehicleRpm = 1800.0 + ((vehicleSpeed - 75.0) / 30.0) * 1400.0;
                            } else if (vehicleSpeed < 140.0) {
                                vehicleRpm = 2000.0 + ((vehicleSpeed - 105.0) / 35.0) * 1300.0;
                            } else {
                                vehicleRpm = 2200.0 + ((vehicleSpeed - 140.0) / 80.0) * 2200.0;
                            }
                            // Realistic instantaneous consumption
                            consumption = 4.2 + (vehicleSpeed / 45.0) * 1.6 + (vehicleRpm / 3500.0) * 1.4;
                        } else {
                            vehicleRpm = 820.0 + (Math.sin(System.currentTimeMillis() / 600.0) * 15.0);
                            consumption = 0.7; // 0.7 l/h idling at standstill
                        }
                    } else {
                        vehicleSpeed = 0.0;
                        vehicleRpm = 0.0;
                        consumption = 0.0;
                    }

                    // Re-read vehicle devices config and enforce Clamp15 / Normal operating state
                    try {
                        Class<?> sw = Class.forName("de.vw.mib.asl.internal.car.viewoption.StateWork", false, cl);
                        Method getDm = sw.getMethod("getDeviceManager");
                        getDm.setAccessible(true);
                        Object dm = getDm.invoke(null);
                        if (dm != null) {
                            try {
                                Method m = dm.getClass().getDeclaredMethod("readDevicesConfig");
                                m.setAccessible(true);
                                m.invoke(dm);
                            } catch (Throwable ignored) {}

                            // Enforce normal ignition / standstill operating state in DeviceManager
                            try {
                                Field fClamp = dm.getClass().getDeclaredField("clamp15");
                                fClamp.setAccessible(true);
                                fClamp.setInt(dm, engineRunning ? 1 : 0);

                                Field fStandstill = dm.getClass().getDeclaredField("standstill");
                                fStandstill.setAccessible(true);
                                fStandstill.setInt(dm, vehicleSpeed < 1.0 ? 1 : 0);

                                Field fSpeed = dm.getClass().getDeclaredField("speedTooHigh");
                                fSpeed.setAccessible(true);
                                fSpeed.setInt(dm, 0);

                                Method getStateNormal = dm.getClass().getDeclaredMethod("getStateNormal");
                                getStateNormal.setAccessible(true);
                                Object normalState = getStateNormal.invoke(dm);

                                Method setState = dm.getClass().getDeclaredMethod("setState", Class.forName("de.vw.mib.asl.internal.car.dm.states.VehicleState", false, cl));
                                setState.setAccessible(true);
                                setState.invoke(dm, normalState);

                                Method getAllCodedDevices = dm.getClass().getDeclaredMethod("getAllCodedDevices");
                                getAllCodedDevices.setAccessible(true);
                                Object codedDevices = getAllCodedDevices.invoke(dm);

                                Method notifyMethod = dm.getClass().getDeclaredMethod("notify", Class.forName("de.vw.mib.collections.ints.IntArrayList", false, cl), int.class);
                                notifyMethod.setAccessible(true);
                                notifyMethod.invoke(dm, codedDevices, 0); // 0 = DEVICE_AVAILABLE_REASON_NORMAL
                            } catch (Throwable ignored) {}
                        }
                    } catch (Throwable ignored) {}

                    // Inform HsmTargetCarViewOption of ignition clamp15 state
                    try {
                        Class<?> sm = Class.forName("de.vw.mib.asl.framework.internal.framework.ServiceManager", false, cl);
                        Object ge = sm.getField("eventMain").get(null);
                        if (ge != null) {
                            Class<?> geIfc = Class.forName("de.vw.mib.genericevents.GenericEvents", false, cl);
                            Class<?> edIfc = Class.forName("de.vw.mib.genericevents.EventDispatcherHSM", false, cl);
                            Method getEd = geIfc.getMethod("getEventDispatcher");
                            Object ed = getEd.invoke(ge);
                            Method getTarget = edIfc.getMethod("getTarget", int.class);
                            Object voTarget = getTarget.invoke(ed, 1200009); // ASL_CAR_VIEWOPTION_TARGET
                            if (voTarget != null) {
                                Method informClamp15 = voTarget.getClass().getDeclaredMethod("informClamp15", boolean.class);
                                informClamp15.setAccessible(true);
                                informClamp15.invoke(voTarget, engineRunning);
                            }
                        }
                    } catch (Throwable ignored) {}

                    // Set CLAMP15ON, VEHICLE_MOVING, and Bordcomputer validity flags in ASLPropertyManager
                    try {
                        Class<?> smCls = Class.forName("de.vw.mib.asl.framework.internal.framework.ServiceManager", false, cl);
                        Object propMgr = smCls.getField("aslPropertyManager").get(null);
                        if (propMgr != null) {
                            Method valChangedBool = propMgr.getClass().getMethod("valueChangedBoolean", int.class, boolean.class);
                            valChangedBool.invoke(propMgr, 1651, engineRunning); // System CLAMP15ACTIVE
                            valChangedBool.invoke(propMgr, 1678, true); // System PETROL_ENGINE_AVAILABLE
                            valChangedBool.invoke(propMgr, 10583, engineRunning); // CLAMP15ON
                            valChangedBool.invoke(propMgr, 10563, vehicleSpeed >= 1.0); // VEHICLE_MOVING
                            valChangedBool.invoke(propMgr, 10132, false); // ECO_COMFORT_INITIALIZING
                            valChangedBool.invoke(propMgr, 10819, true);  // BC_MENU1CONSUMPTION_VALUE_VALID
                            valChangedBool.invoke(propMgr, 10820, true);  // BC_MENU2CONSUMPTION_VALUE_VALID
                            valChangedBool.invoke(propMgr, 10821, true);  // BC_MENU3CONSUMPTION_VALUE_VALID
                            valChangedBool.invoke(propMgr, 10822, true);  // BC_MENU1CONSUMPTION_VALUE_VALID__BI
                            valChangedBool.invoke(propMgr, 10823, true);  // BC_MENU2CONSUMPTION_VALUE_VALID__BI
                            valChangedBool.invoke(propMgr, 10824, true);  // BC_MENU3CONSUMPTION_VALUE_VALID__BI
                            valChangedBool.invoke(propMgr, 10537, true);  // OIL_TEMPERATURE_VALID
                            valChangedBool.invoke(propMgr, 10540, true);  // BC_COOLANT_TEMPERATURE_VALID
                            valChangedBool.invoke(propMgr, 10541, true);  // BCL_OIL_TEMPERATURE_EXISTENT
                            valChangedBool.invoke(propMgr, 10542, true);  // BC_COOLANT_TEMPERATURE_EXISTENT
                            valChangedBool.invoke(propMgr, 10113, true);  // TBT_EXISTENT (ThinkBlue)
                            valChangedBool.invoke(propMgr, 10114, true);  // OFFROAD_EXISTENT
                        }
                    } catch (Throwable ignored) {}

                    Object disp = dispCls.getMethod("getInstance").invoke(null);
                    if (disp != null) {
                        int[] targetEvents = (s_bootLoops <= 25 || s_forceRefresh) ? carEvents : dynamicEvents;
                        s_forceRefresh = false;
                        for (int evt : targetEvents) {
                            try {
                                @SuppressWarnings("unchecked")
                                List<?> apps = (List<?>) mapIdToApp.invoke(disp, evt);
                                if (apps != null) {
                                    for (Object app : apps) {
                                        refreshService(cl, app);
                                    }
                                }
                            } catch (Throwable ignored) {}
                        }
                    }
                    Thread.sleep(1000); // 1 Hz vehicle telemetry update
                }
            } catch (Throwable t) {
                System.out.println("[VehicleSim] Error: " + t);
            }
        }, "TrackA-VehicleSim").start();
    }

    private static void initCarData(ClassLoader cl) {
        try {
            try {
                Class<?> mdsm = Class.forName("de.vw.mib.asl.internal.car.model.ModelDeviceStartupManager", false, cl);
                mdsm.getMethod("start").invoke(null);
            } catch (Throwable ignored) {}

            Class<?> lmCls = Class.forName("generated.de.vw.mib.asl.internal.ListManager", false, cl);
            Method getGenericList = lmCls.getMethod("getGenericASLList", int.class);

            Class<?> vhCls = Class.forName("de.vw.mib.asl.internal.car.bordcomputer.transformer.CarBCMenuValueHolder", false, cl);
            Constructor<?> vhCtor = vhCls.getConstructor(int.class, int.class, int.class);

            // Authentic Golf 7 BCMENU list value order:
            // Index 0: Driven distance (itemUnit: 0 = KM)
            // Index 1: Average speed (itemUnit: 3 = KM_H)
            // Index 2: Driving time (itemUnit: 2 = H)
            // Index 3: Average consumption (itemUnit: 5 = L_100KM)
            // Index 4: Cruising range (itemUnit: 0 = KM)
            int dKm = (int) tripDistance;
            int dDec = (int) Math.round((tripDistance - dKm) * 10);
            int cL = (int) avgConsumption;
            int cDec = (int) Math.round((avgConsumption - cL) * 10);
            int hrs = tripMinutes / 60;
            int mins = tripMinutes % 60;

            Object[][] menus = {
                { // BCMenu1: Since start
                    vhCtor.newInstance(dKm, dDec, 0),        // Distance km
                    vhCtor.newInstance((int) avgSpeed, 0, 3), // Speed km/h
                    vhCtor.newInstance(hrs, mins, 2),        // Time h:m
                    vhCtor.newInstance(cL, cDec, 5),         // Consumption l/100km
                    vhCtor.newInstance(range, 0, 0)          // Range km
                },
                { // BCMenu2: Long-term
                    vhCtor.newInstance((int) odometer, 0, 0),
                    vhCtor.newInstance(58, 0, 3),
                    vhCtor.newInstance(31, 30, 2),
                    vhCtor.newInstance(6, 8, 5),
                    vhCtor.newInstance(range, 0, 0)
                },
                { // BCMenu3: Since refueling
                    vhCtor.newInstance(182, 4, 0),
                    vhCtor.newInstance(52, 0, 3),
                    vhCtor.newInstance(3, 23, 2),
                    vhCtor.newInstance(6, 2, 5),
                    vhCtor.newInstance(range, 0, 0)
                }
            };

            int[] listIds = { 10136, 10137, 10138, 10139, 10140, 10141 };
            for (int i = 0; i < listIds.length; i++) {
                Object list = getGenericList.invoke(null, listIds[i]);
                if (list != null) {
                    try { list.getClass().getMethod("forceUnfreeze").invoke(list); } catch (Throwable ignored) {}
                    try { list.getClass().getMethod("updateList", Object[].class).invoke(list, (Object) menus[i % 3]); } catch (Throwable ignored) {}
                }
            }

            Class<?> fsCls = Class.forName("de.vw.mib.asl.internal.car.model.FunctionState", false, cl);
            Constructor<?> fsCtor = fsCls.getConstructor(int.class, int.class);

            Object[] bcFuncStates11 = (Object[]) Array.newInstance(fsCls, 11);
            for (int k = 0; k < 11; k++) {
                bcFuncStates11[k] = fsCtor.newInstance(2, 0); // STATE_NORMAL_OPERATION = 2, reason = 0
            }

            Object[] carFuncStates16 = (Object[]) Array.newInstance(fsCls, 16);
            for (int k = 0; k < 16; k++) {
                carFuncStates16[k] = fsCtor.newInstance(2, 0);
            }

            // Exactly 19 functions for authentic Golf 7 CAR SETUP (ASLCarPropertyManager C0..C18)
            Object[] carFuncStates19 = (Object[]) Array.newInstance(fsCls, 19);
            for (int k = 0; k < 19; k++) {
                carFuncStates19[k] = fsCtor.newInstance(k == 0 ? 0 : 2, 0); // C0_ELECTRICALDRIVING=0, rest=2 (available)
            }

            // Exactly 14 functions for FACTORY SETTINGS
            Object[] factoryStates14 = (Object[]) Array.newInstance(fsCls, 14);
            for (int k = 0; k < 14; k++) {
                factoryStates14[k] = fsCtor.newInstance(k == 1 ? 0 : 2, 0);
            }

            int[] bcLists = { 10807, 10808, 10809 };
            for (int lid : bcLists) {
                Object list = getGenericList.invoke(null, lid);
                if (list != null) {
                    try { list.getClass().getMethod("forceUnfreeze").invoke(list); } catch (Throwable ignored) {}
                    try { list.getClass().getMethod("updateList", Object[].class).invoke(list, (Object) bcFuncStates11); } catch (Throwable ignored) {}
                }
            }

            int[] mainLists = { 10803, 10804, 10805 };
            for (int lid : mainLists) {
                Object list = getGenericList.invoke(null, lid);
                if (list != null) {
                    try { list.getClass().getMethod("forceUnfreeze").invoke(list); } catch (Throwable ignored) {}
                    try { list.getClass().getMethod("updateList", Object[].class).invoke(list, (Object) carFuncStates16); } catch (Throwable ignored) {}
                }
            }

            int[] setupLists = { 10791, 10792, 10793 };
            for (int lid : setupLists) {
                Object list = getGenericList.invoke(null, lid);
                if (list != null) {
                    try { list.getClass().getMethod("forceUnfreeze").invoke(list); } catch (Throwable ignored) {}
                    try { list.getClass().getMethod("updateList", Object[].class).invoke(list, (Object) carFuncStates19); } catch (Throwable ignored) {}
                }
            }

            int[] factLists = { 10771, 10772, 10773 };
            for (int lid : factLists) {
                Object list = getGenericList.invoke(null, lid);
                if (list != null) {
                    try { list.getClass().getMethod("forceUnfreeze").invoke(list); } catch (Throwable ignored) {}
                    try { list.getClass().getMethod("updateList", Object[].class).invoke(list, (Object) factoryStates14); } catch (Throwable ignored) {}
                }
            }

            // Populate Service and Oil inspection intervals for authentic Golf 7 Car Service menu
            try {
                Class<?> srvCls = Class.forName("generated.de.vw.mib.asl.internal.car.service.transformer.CarServiceSIAServiceCollector", false, cl);
                Object srvColl = srvCls.getDeclaredConstructor().newInstance();
                srvCls.getField("car_service_sia_service_time_value").setInt(srvColl, 480);
                srvCls.getField("car_service_sia_service_time_status").setInt(srvColl, 0);
                srvCls.getField("car_service_sia_service_distance_value").setInt(srvColl, 28400);
                srvCls.getField("car_service_sia_service_distance_unit").setInt(srvColl, 0);
                srvCls.getField("car_service_sia_service_distance_status").setInt(srvColl, 0);
                Object srvList = getGenericList.invoke(null, 10267);
                if (srvList != null) {
                    try { srvList.getClass().getMethod("forceUnfreeze").invoke(srvList); } catch (Throwable ignored) {}
                    Object arr = Array.newInstance(srvCls, 1);
                    Array.set(arr, 0, srvColl);
                    srvList.getClass().getMethod("updateList", Object[].class).invoke(srvList, arr);
                }
            } catch (Throwable ignored) {}

            try {
                Class<?> oilCls = Class.forName("generated.de.vw.mib.asl.internal.car.service.transformer.CarServiceSIAOilCollector", false, cl);
                Object oilColl = oilCls.getDeclaredConstructor().newInstance();
                oilCls.getField("car_service_sia_oil_time_value").setInt(oilColl, 240);
                oilCls.getField("car_service_sia_oil_time_status").setInt(oilColl, 0);
                oilCls.getField("car_service_sia_oil_distance_value").setInt(oilColl, 14200);
                oilCls.getField("car_service_sia_oil_distance_unit").setInt(oilColl, 0);
                oilCls.getField("car_service_sia_oil_distance_status").setInt(oilColl, 0);
                Object oilList = getGenericList.invoke(null, 10268);
                if (oilList != null) {
                    try { oilList.getClass().getMethod("forceUnfreeze").invoke(oilList); } catch (Throwable ignored) {}
                    Object arr = Array.newInstance(oilCls, 1);
                    Array.set(arr, 0, oilColl);
                    oilList.getClass().getMethod("updateList", Object[].class).invoke(oilList, arr);
                }
            } catch (Throwable ignored) {}

            // Initialize ASLCarContainer static arrays and instance settings
            Class<?> ccCls = Class.forName("de.vw.mib.asl.internal.car.ASLCarContainer", false, cl);
            Object cc = ccCls.getMethod("getInstance").invoke(null);
            if (cc != null) {
                try { ccCls.getMethod("setNumberOfWindows", int.class).invoke(cc, 4); } catch (Throwable ignored) {}
                try { ccCls.getMethod("setSunRoofAvailable", boolean.class).invoke(cc, true); } catch (Throwable ignored) {}
            }

            boolean[] fe = (boolean[]) ccCls.getField("functionExistent").get(null);
            if (fe == null || fe.length < 222) {
                fe = new boolean[222];
                ccCls.getField("functionExistent").set(null, fe);
            }
            for (int i = 0; i < fe.length; i++) {
                fe[i] = (i != 0); // electric driving false, all 221 others true
            }

            Class<?> cfcCls = Class.forName("de.vw.mib.asl.internal.car.transformer.CarFunctionCollector", false, cl);
            Constructor<?> cfcCtor = cfcCls.getConstructor(boolean.class, int.class);
            Object[] coll = (Object[]) ccCls.getField("collector").get(null);
            if (coll == null || coll.length < 222) {
                coll = (Object[]) Array.newInstance(cfcCls, 222);
                ccCls.getField("collector").set(null, coll);
            }
            for (int i = 0; i < coll.length; i++) {
                coll[i] = cfcCtor.newInstance(i != 0, 0); // available = true, reason = 0 (NORMAL)
            }

            Object list10126 = getGenericList.invoke(null, 10126);
            if (list10126 != null) {
                try { list10126.getClass().getMethod("forceUnfreeze").invoke(list10126); } catch (Throwable ignored) {}
                try { list10126.getClass().getMethod("updateList", Object[].class).invoke(list10126, (Object) coll); } catch (Throwable ignored) {}
            }

            boolean[] menuCoding = null;
            if (cc != null) {
                try {
                    menuCoding = (boolean[]) ccCls.getMethod("getMenuCoding").invoke(cc);
                    if (menuCoding != null) {
                        for (int i = 0; i < menuCoding.length; i++) {
                            menuCoding[i] = (i != 0);
                        }
                    }
                } catch (Throwable ignored) {}
            }

            // Update ASL Property Manager for all CAR view existence and menu coding
            Class<?> smCls = Class.forName("de.vw.mib.asl.framework.internal.framework.ServiceManager", false, cl);
            Object propMgr = smCls.getField("aslPropertyManager").get(null);
            if (propMgr != null) {
                Method valChangedFlagVector = propMgr.getClass().getMethod("valueChangedFlagVector", int.class, boolean[].class);
                Method valChangedBool = propMgr.getClass().getMethod("valueChangedBoolean", int.class, boolean.class);
                Method valChangedInt = propMgr.getClass().getMethod("valueChangedInteger", int.class, int.class);

                valChangedFlagVector.invoke(propMgr, 10106, fe); // FUNCTION_EXISTENT_STATE
                if (menuCoding != null) {
                    valChangedFlagVector.invoke(propMgr, 10111, menuCoding); // MENU_CODING_STATE
                }

                valChangedBool.invoke(propMgr, 10115, true); // SPORT_EXISTENT
                valChangedBool.invoke(propMgr, 10116, true); // BORD_COMPUTER_EXISTENT
                valChangedBool.invoke(propMgr, 10117, true); // EXISTENT__BORD_COMPUTER_TRIP
                valChangedBool.invoke(propMgr, 10118, true); // EXISTENT__BORD_COMPUTER_LONGTERM
                valChangedBool.invoke(propMgr, 10119, true); // EXISTENT__BORD_COMPUTER_FUELCYCLE
                valChangedBool.invoke(propMgr, 10121, true); // EXISTENT__ECO
                valChangedBool.invoke(propMgr, 10122, true); // EXISTENT__VEHICLE_STATUS
                valChangedBool.invoke(propMgr, 10123, true); // EXISTENT__VEHICLE_STATUS_MAIN
                valChangedBool.invoke(propMgr, 10124, true); // EXISTENT__VEHICLE_STATUS_TIRES
                valChangedBool.invoke(propMgr, 10125, true); // EXISTENT__VEHICLE_STATUS_START_STOP
                valChangedBool.invoke(propMgr, 10583, true); // CLAMP15ON (IGNITION ON!)
                valChangedBool.invoke(propMgr, 10107, true); // SYSTEM_SETUP_TIME_DATE_FUNCTION_AVAILABLE_STATE
                valChangedInt.invoke(propMgr, 10108, 0);     // SYSTEM_SETUP_TIME_DATE_FUNCTION_AVAILABLE_REASON
                valChangedBool.invoke(propMgr, 10109, true); // SYSTEM_SETUP_UNITS_FUNCTION_AVAILABLE_STATE
                valChangedInt.invoke(propMgr, 10110, 0);     // SYSTEM_SETUP_UNITS_FUNCTION_AVAILABLE_REASON

                // Live Telemetry Properties
                valChangedInt.invoke(propMgr, 10143, range);
                valChangedInt.invoke(propMgr, 10145, fuel);
                valChangedInt.invoke(propMgr, 10147, (int) odometer);
                valChangedInt.invoke(propMgr, 10148, (int) tripDistance);
            }
        } catch (Throwable t) {
            // Keep going
        }
    }

    private static boolean navInitialized = false;

    private static void initNavigationData(ClassLoader cl) {
        if (navInitialized) return;
        try {
            Class<?> factoryCls = Class.forName("de.vw.mib.asl.api.navigation.startup.ASLNavigationStartupFactory", false, cl);
            Object navApi = factoryCls.getMethod("getNavigationStartupApi").invoke(null);
            if (navApi != null) {
                navApi.getClass().getMethod("sendEvNaviStatusChange", boolean.class).invoke(navApi, true);
            }
            Class<?> smCls = Class.forName("de.vw.mib.asl.framework.internal.framework.ServiceManager", false, cl);
            Object propMgr = smCls.getField("aslPropertyManager").get(null);
            if (propMgr != null) {
                Method valueChangedInt = propMgr.getClass().getMethod("valueChangedInteger", int.class, int.class);
                Method valueChangedBool = propMgr.getClass().getMethod("valueChangedBoolean", int.class, boolean.class);
                valueChangedInt.invoke(propMgr, 757, 0); // DATABASE_VALID = 0 (VALID)
                valueChangedInt.invoke(propMgr, 753, 100); // DATABASE_PROGRESS = 100
                valueChangedBool.invoke(propMgr, 758, true); // LOADED = true
                valueChangedBool.invoke(propMgr, 759, true); // MAP_READY = true
                try {
                    valueChangedInt.invoke(propMgr, 1676, 1); // OPERATIONSTATE_NORMAL = 1
                } catch (Throwable ignored) {}
                navInitialized = true;
            }
        } catch (Throwable t) {}
    }

    private static void initPhoneData(ClassLoader cl) {
        try {
            Class<?> smCls = Class.forName("de.vw.mib.asl.framework.internal.framework.ServiceManager", false, cl);
            Object propMgr = smCls.getField("aslPropertyManager").get(null);
            if (propMgr != null) {
                Method valChangedInt = propMgr.getClass().getMethod("valueChangedInteger", int.class, int.class);
                Method valChangedBool = propMgr.getClass().getMethod("valueChangedBoolean", int.class, boolean.class);
                Method valChangedStr = propMgr.getClass().getMethod("valueChangedString", int.class, String.class);

                valChangedInt.invoke(propMgr, 2906, 4); // ACTIVATION_STATE__1: C4_ACTIVATIONSTATE_PHONE_ON
                valChangedStr.invoke(propMgr, 1084, "Demo phone"); // NETWORK_NAME: Demo phone
                valChangedInt.invoke(propMgr, 1085, 1); // NETWORK_STATE: REGISTERED_AT_PROVIDER_NETWORK
                valChangedInt.invoke(propMgr, 1086, 3); // NETWORK_TYPE: 4G
                valChangedInt.invoke(propMgr, 1087, 5); // SIGNAL_STRENGTH: 5 bars
                valChangedBool.invoke(propMgr, 1097, true); // SIGNAL_STRENGTH_AVAILABLE
                valChangedInt.invoke(propMgr, 1082, 2); // LOCK_STATE: C2_LOCKSTATE_NO_LOCK
                valChangedInt.invoke(propMgr, 1410012, 5); // BATTERY_LEVEL__1: 5 bars
            }
        } catch (Throwable ignored) {}
    }

    private static void initMediaData(ClassLoader cl) {
        try {
            Class<?> smCls = Class.forName("de.vw.mib.asl.framework.internal.framework.ServiceManager", false, cl);
            Object propMgr = smCls.getField("aslPropertyManager").get(null);
            if (propMgr != null) {
                Method valChangedInt = propMgr.getClass().getMethod("valueChangedInteger", int.class, int.class);
                Method valChangedStr = propMgr.getClass().getMethod("valueChangedString", int.class, String.class);
                Method valChangedBool = propMgr.getClass().getMethod("valueChangedBoolean", int.class, boolean.class);

                valChangedInt.invoke(propMgr, 8, 8); // CURRENT_AUDIO_SOURCE: C8_USB
                valChangedStr.invoke(propMgr, 1, "USB"); // ACTIVE_SOURCE_NAME: "USB"
                valChangedInt.invoke(propMgr, 29, 1); // PLAYER_STATE: C1_PLAY
                valChangedInt.invoke(propMgr, 2593, 1); // USB_STATE: C1_USB_OK
                valChangedBool.invoke(propMgr, 47, true); // PLAY_PAUSE_SUPPORTED
                valChangedBool.invoke(propMgr, 48, true); // PLAYTIME_SUPPORTED
                valChangedBool.invoke(propMgr, 55, true); // SKIP_FORWARD_SUPPORTED
                valChangedBool.invoke(propMgr, 54, true); // SKIP_BACKWARD_SUPPORTED
                valChangedInt.invoke(propMgr, 13, 42); // CURRENT_TRACK_ELAPSED_TIME (42s)
                valChangedInt.invoke(propMgr, 14, 144); // CURRENT_TRACK_REMAINING_TIME (144s)
                try { valChangedStr.invoke(propMgr, 7, "/Music"); } catch (Throwable ignored) {}
            }

            Class<?> lmCls = Class.forName("generated.de.vw.mib.asl.internal.ListManager", false, cl);
            Method getGenericList = lmCls.getMethod("getGenericASLList", int.class);
            Object trackList = getGenericList.invoke(null, 58); // CURRENT_TRACK_INFO
            if (trackList != null) {
                Class<?> ctiCls = Class.forName("generated.de.vw.mib.asl.internal.avdc.audio.transformer.AVDCAudioCurrentTrackInfoCollector", false, cl);
                Object cti = ctiCls.getDeclaredConstructor().newInstance();
                ctiCls.getField("avdc_audio_current_track_info__artist").set(cti, "Imagine Dragons");
                ctiCls.getField("avdc_audio_current_track_info__title").set(cti, "Radioactive");
                ctiCls.getField("avdc_audio_current_track_info__album").set(cti, "Night Visions");
                ctiCls.getField("avdc_audio_current_track_info__tracknumber").setInt(cti, 1);
                ctiCls.getField("avdc_audio_current_track_info__total_time").setInt(cti, 186);
                ctiCls.getField("avdc_audio_current_track_info__filename").set(cti, "01-Radioactive.mp3");
                ctiCls.getField("avdc_audio_current_track_info__is_cover_available").setBoolean(cti, false);
                trackList.getClass().getMethod("forceUnfreeze").invoke(trackList);
                Object ctiArr = Array.newInstance(ctiCls, 1);
                Array.set(ctiArr, 0, cti);
                trackList.getClass().getMethod("updateList", Object[].class).invoke(trackList, (Object) ctiArr);
            }
        } catch (Throwable ignored) {}
    }

    private static void refreshService(ClassLoader cl, Object impl) {
        if (impl == null) return;
        try {
            Method getAllAttrs = impl.getClass().getDeclaredMethod("getAllAttributes");
            getAllAttrs.setAccessible(true);
            int[] attrs = (int[]) getAllAttrs.invoke(impl);
            if (attrs == null) return;

            Method sendDummyData = null;
            for (Class<?> k = impl.getClass(); k != null; k = k.getSuperclass()) {
                try {
                    sendDummyData = k.getDeclaredMethod("sendDummyData", int.class, Class.forName("org.dsi.ifc.base.DSIListener", false, cl));
                    sendDummyData.setAccessible(true);
                    break;
                } catch (NoSuchMethodException e) {}
            }
            if (sendDummyData == null) return;

            for (int attr : attrs) {
                @SuppressWarnings("unchecked")
                List<Object> listeners = (List<Object>) listenersByAttr.invoke(impl, attr);
                if (listeners == null || listeners.isEmpty()) continue;

                Class<?>[] ifcs = getListenerInterfaces(cl, impl, listeners);
                if (ifcs.length == 0) continue;

                Method finalSendDummy = sendDummyData;
                InvocationHandler handler = (proxy, method, args) -> {
                    if (method.getDeclaringClass() == Object.class) return null;
                    if (args != null && args.length > 0) {
                        // Universal validity flag conversion:
                        // Convert any trailing validity flag from INVALID (2) to VALID (1)
                        if (args[args.length - 1] instanceof Integer last && last.intValue() == 2) {
                            args[args.length - 1] = Integer.valueOf(VALID);
                        }

                        Class<?>[] pt = method.getParameterTypes();
                        for (int i = 0; i < args.length; i++) {
                            if (args[i] == null && i < pt.length) {
                                Class<?> pType = pt[i];
                                if (pType.isArray()) {
                                    args[i] = java.lang.reflect.Array.newInstance(pType.getComponentType(), 0);
                                } else if (!pType.isPrimitive() && !pType.isInterface()) {
                                    try {
                                        Constructor<?> ctor = pType.getDeclaredConstructor();
                                        ctor.setAccessible(true);
                                        args[i] = ctor.newInstance();
                                    } catch (Throwable ignored) {}
                                }
                            }
                        }

                        // Apply authentic Golf 7 settings
                        for (Object a : args) {
                            if (a != null) {
                                enableFeatures(a);
                            }
                        }

                        applyTelemetry(cl, method.getName(), args);
                        applyClockTelemetry(method.getName(), args);
                        fixFullRangeUpdate(method.getName(), args);
                    }

                    // Drop popup-spamming Bluetooth events
                    String mName = method.getName();
                    if (mName.equals("updateIncomingServiceRequest") ||
                        mName.equals("updatePasskeyState") ||
                        mName.equals("updateMasterRoleRequestError")) {
                        return null;
                    }

                    for (Object l : listeners) {
                        try {
                            method.invoke(l, args);
                        } catch (Throwable ignored) {}
                    }
                    return null;
                };

                Object proxy = Proxy.newProxyInstance(cl, ifcs, handler);
                finalSendDummy.invoke(impl, attr, proxy);
            }
        } catch (Throwable ignored) {}
    }

    private static Class<?>[] getListenerInterfaces(ClassLoader cl, Object impl, List<Object> listeners) {
        Set<Class<?>> set = new LinkedHashSet<>();
        try {
            Class<?> dsiListener = Class.forName("org.dsi.ifc.base.DSIListener", false, cl);
            set.add(dsiListener);
            for (Class<?> c = impl.getClass(); c != null; c = c.getSuperclass()) {
                for (Class<?> ifc : c.getInterfaces()) {
                    String name = ifc.getName();
                    if (name.endsWith("Listener")) {
                        set.add(ifc);
                    } else {
                        try {
                            Class<?> lCls = Class.forName(name + "Listener", false, cl);
                            set.add(lCls);
                        } catch (Throwable ignored) {}
                    }
                }
            }
            if (listeners != null) {
                for (Object l : listeners) {
                    if (l != null) {
                        for (Class<?> c = l.getClass(); c != null; c = c.getSuperclass()) {
                            for (Class<?> ifc : c.getInterfaces()) {
                                if (dsiListener.isAssignableFrom(ifc)) {
                                    set.add(ifc);
                                }
                            }
                        }
                    }
                }
            }
        } catch (Throwable ignored) {}
        return set.toArray(new Class<?>[0]);
    }

    private static void enableFeatures(Object obj) {
        if (obj == null) return;
        if (obj instanceof org.dsi.ifc.global.CarViewOption) {
            org.dsi.ifc.global.CarViewOption cvo = (org.dsi.ifc.global.CarViewOption) obj;
            cvo.state = NORMAL_OPERATION; // 2
            cvo.reason = 0; // NORMAL
            return;
        }
        if (obj instanceof org.dsi.ifc.carparkingsystem.ParkingSystemViewOptions) {
            org.dsi.ifc.carparkingsystem.ParkingSystemViewOptions ps = (org.dsi.ifc.carparkingsystem.ParkingSystemViewOptions) obj;
            if (ps.pdcConfiguration == null) {
                ps.pdcConfiguration = new org.dsi.ifc.carparkingsystem.PDCConfiguration((byte) 4, (byte) 4, (byte) 0, (byte) 0, 2637, true, true);
            } else {
                ps.pdcConfiguration.numberFrontSectors = 4;
                ps.pdcConfiguration.numberRearSectors = 4;
                ps.pdcConfiguration.wheelbase = 2637;
                ps.pdcConfiguration.crashWarning = true;
                ps.pdcConfiguration.steeringInformation = true;
            }
        }
        Class<?> cls = obj.getClass();
        String cName = cls.getSimpleName();
        if (NOT_ON_ICE.matcher(cName).find()) return;

        for (Field f : cls.getFields()) {
            try {
                f.setAccessible(true);
                String fName = f.getName();
                Class<?> type = f.getType();

                if (NOT_ON_ICE.matcher(fName).find()) {
                    if (type == boolean.class) f.setBoolean(obj, false);
                    else if (type == int.class) f.setInt(obj, 0);
                    else if (type.getName().equals("org.dsi.ifc.global.CarViewOption")) {
                        try {
                            org.dsi.ifc.global.CarViewOption cvo = (org.dsi.ifc.global.CarViewOption) f.get(obj);
                            if (cvo == null) {
                                cvo = new org.dsi.ifc.global.CarViewOption(0, 0);
                                f.set(obj, cvo);
                            } else {
                                cvo.state = 0;
                                cvo.reason = 0;
                            }
                        } catch (Throwable ignored) {}
                    }
                    continue;
                }

                if (type.getName().equals("org.dsi.ifc.global.CarViewOption")) {
                    try {
                        org.dsi.ifc.global.CarViewOption cvo = (org.dsi.ifc.global.CarViewOption) f.get(obj);
                        if (cvo == null) {
                            cvo = new org.dsi.ifc.global.CarViewOption(NORMAL_OPERATION, 0);
                            f.set(obj, cvo);
                        } else {
                            cvo.state = NORMAL_OPERATION;
                            cvo.reason = 0;
                        }
                    } catch (Throwable ignored) {}
                    continue;
                }

                if (FLAG_CLASSES.matcher(cName).matches()) {
                    if (type == boolean.class) {
                        f.setBoolean(obj, true);
                    } else if (type == int.class) {
                        if (fName.equalsIgnoreCase("state") || fName.endsWith("State")) {
                            f.setInt(obj, NORMAL_OPERATION);
                        } else {
                            f.setInt(obj, 1);
                        }
                    }
                } else {
                    if (type == int.class) {
                        if (fName.startsWith("btServiceRequest") || fName.startsWith("btPasskey") || fName.contains("reconnect")) {
                            f.setInt(obj, 0);
                            continue;
                        }
                        if (fName.equalsIgnoreCase("state") || fName.endsWith("State")) {
                            if (cName.contains("ViewOption") || fName.contains("viewOption") || fName.contains("ViewOption")) {
                                f.setInt(obj, NORMAL_OPERATION);
                            } else {
                                f.setInt(obj, VALID);
                            }
                        } else if (fName.equalsIgnoreCase("reason") || fName.endsWith("Reason")) {
                            f.setInt(obj, 0);
                        }
                    }
                }

                if (!type.isPrimitive() && !type.isArray() && !type.isInterface() && type.getName().startsWith("org.dsi.ifc.")) {
                    Object child = f.get(obj);
                    if (child == null) {
                        try {
                            Constructor<?> ctor = type.getDeclaredConstructor();
                            ctor.setAccessible(true);
                            child = ctor.newInstance();
                            f.set(obj, child);
                        } catch (Throwable ignored) {}
                    }
                    if (child != null) {
                        enableFeatures(child);
                    }
                }
            } catch (Throwable ignored) {}
        }
    }

    private static void applyTelemetry(ClassLoader cl, String method, Object[] args) {
        if (args == null || args.length == 0) return;
        try {
            // Power Management & Ignition
            if (method.equals("updateClampSignal")) {
                Object c = args[0];
                if (c != null) {
                    c.getClass().getField("clampS").setBoolean(c, engineRunning);
                    c.getClass().getField("clamp15").setBoolean(c, engineRunning);
                    c.getClass().getField("clampX").setBoolean(c, engineRunning);
                    c.getClass().getField("clamp50").setBoolean(c, false);
                }
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updatePowerManagementState") || method.equals("updatePowerManagementStateRight")) {
                args[0] = 1; // POWERSTATE_ON
                if (args.length > 1) args[1] = 0; // reason
                if (args.length > 2) args[2] = VALID;
                return;
            }

            // General Vehicle States
            if (method.equals("updateVehicleStandstill")) {
                args[0] = Boolean.valueOf(vehicleSpeed < 1.0);
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateVehicleSpeed") || method.equals("updateCurrentVehicleSpeed")) {
                args[0] = Float.valueOf((float) vehicleSpeed);
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateCarVelocityThreshold")) {
                args[0] = Boolean.FALSE;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateParkingBrake")) {
                args[0] = Boolean.valueOf(vehicleSpeed < 1.0);
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateReverseGear")) {
                args[0] = Boolean.FALSE;
                if (args.length > 1) args[1] = VALID;
                return;
            }

            // Board Computer configuration
            if (method.equals("updateBCViewOptions") && args[0] != null) {
                Object bc = args[0];
                Object cfg = bc.getClass().getField("configuration").get(bc);
                if (cfg == null) {
                    cfg = Class.forName("org.dsi.ifc.carkombi.BCConfiguration", false, cl).getDeclaredConstructor().newInstance();
                    bc.getClass().getField("configuration").set(bc, cfg);
                }
                cfg.getClass().getField("primaryEngineType").setInt(cfg, 7); // PETROL_GASOLINE
                cfg.getClass().getField("secondaryEngineType").setInt(cfg, 0);
                if (args.length > 1) args[1] = VALID;
                return;
            }

            // SWaP licences: Sport HMI FeC (0x60400)
            if (method.equals("updateFscList") && args[0] != null) {
                Class<?> fscCls = Class.forName("org.dsi.ifc.swap.SFscStatus", false, cl);
                Constructor<?> fscCtor = fscCls.getConstructor(int.class, int.class, int.class);
                Object fsc = fscCtor.newInstance(0x60400, 0, 0); // Sport HMI installed & valid
                Object fscArr = Array.newInstance(fscCls, 1);
                Array.set(fscArr, 0, fsc);
                args[0] = fscArr;
                if (args.length > 1) args[1] = VALID;
                return;
            }

            // Smartphone Integration licences (MirrorLink 1 | CarPlay 2 | Android Auto 4)
            if (method.equals("updateSWaPStatus")) {
                args[0] = 1 | 2 | 4;
                if (args.length > 1) args[1] = VALID;
                return;
            }

            // Profile State (READY)
            if (method.equals("updateProfileState") && args.length >= 3) {
                args[0] = 2; // PROFILESTATE_CHANGE_READY
                args[args.length - 1] = VALID;
                return;
            }

            // Driver Assistance AWV
            if (method.equals("updateAWVViewOptions") && args[0] != null) {
                Object awv = args[0];
                Object cfg = awv.getClass().getField("configuration").get(awv);
                if (cfg != null) {
                    cfg.getClass().getField("systemMode").setInt(cfg, 1); // Front Assist
                }
                if (args.length > 1) args[1] = VALID;
                return;
            }

            // ESC / Brake mode
            if (method.equals("updateBrakeEscMode")) {
                args[0] = 3; // BRAKEESCMODE_FULL (ESC on)
                if (args.length > 1) args[1] = VALID;
                return;
            }

            // Parking system & OPS
            if (method.equals("updatePDCSystemOnOff") || method.equals("updateVPSSystemOnOff") || method.equals("updateVPSSystemState")) {
                args[0] = Boolean.TRUE;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateVPSExtCamConfig")) {
                args[0] = 2; // Manual 5th camera button
                if (args.length > 1) args[1] = VALID;
                return;
            }

            // Navigation
            if (method.equals("updateNavstateOfOperation")) {
                args[0] = 5; // NAVSTATEOFOPERATION_FULLY_OPERABLE
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateReady")) {
                args[0] = Boolean.TRUE;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateZoomList")) {
                args[0] = new float[] { 25.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f, 50000.0f, 100000.0f, 200000.0f };
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateZoomLevel")) {
                args[0] = 20000.0f;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateZoomListIndex")) {
                args[0] = 6;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateMapMode")) {
                args[0] = 1;
                if (args.length > 1) args[1] = VALID;
                return;
            }

            // Audio Management & Availability
            if (method.equals("updateAMAvailable")) {
                args[0] = 3; // AM_AVAILABLE = 3
                if (args.length > 1) args[1] = 0;
                if (args.length > 2) args[2] = VALID;
                return;
            }

            // Media
            if (method.equals("updatePlaybackState")) {
                args[0] = 3; // PLAYBACKSTATE_PLAYING
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updatePlaybackModeList")) {
                Class<?> pmCls = Class.forName("org.dsi.ifc.media.PlaybackMode", false, cl);
                Constructor<?> pmCtor = pmCls.getConstructor(int.class, int.class, int.class);
                Object pmArr = Array.newInstance(pmCls, 1);
                Array.set(pmArr, 0, pmCtor.newInstance(1, 1, 0));
                args[0] = pmArr;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updatePlaybackMode")) {
                args[0] = 1;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateActiveDevice")) {
                args[0] = 1L;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateActiveMedia")) {
                args[0] = 1L;
                if (args.length > 1) args[1] = 1L;
                if (args.length > 2) args[2] = 0;
                if (args.length > 3) args[3] = 0;
                if (args.length > 4) args[4] = VALID;
                return;
            }
            if (method.equals("updateDeviceList")) {
                Class<?> diCls = Class.forName("org.dsi.ifc.media.DeviceInfo", false, cl);
                Constructor<?> diCtor = diCls.getConstructor(long.class, int.class, int.class, int.class);
                Object di = diCtor.newInstance(1L, 2 /* USB */, 1, 1 /* COOPERATIVE */);
                Object diArr = Array.newInstance(diCls, 1);
                Array.set(diArr, 0, di);
                args[0] = diArr;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateMediaList")) {
                Class<?> miCls = Class.forName("org.dsi.ifc.media.MediaInfo", false, cl);
                Class<?> mcCls = Class.forName("org.dsi.ifc.media.MediaCapabilities", false, cl);
                Constructor<?> miCtor = miCls.getConstructor(long.class, long.class, int.class, String.class, String.class, String.class, int.class, mcCls);
                Object mc = mcCls.getDeclaredConstructor().newInstance();
                mcCls.getField("rawBrowser").setBoolean(mc, true);
                mcCls.getField("playerCoverArt").setBoolean(mc, true);
                Object mi = miCtor.newInstance(1L, 1L, 7 /* FILESYSTEM */, "usb-music", "USB", "/media/mp000", 8 | 65536 | 131072, mc);
                Object miArr = Array.newInstance(miCls, 1);
                Array.set(miArr, 0, mi);
                args[0] = miArr;
                if (args.length > 1) args[1] = VALID;
                return;
            }

            // Phone & Bluetooth (Connected with Demo phone)
            if (method.equals("updateDiscoveredDevices")) {
                Class<?> ddCls = Class.forName("org.dsi.ifc.bluetooth.DiscoveredDevice", false, cl);
                Object dd = ddCls.getDeclaredConstructor().newInstance();
                ddCls.getField("deviceName").set(dd, "Demo phone");
                ddCls.getField("deviceAddress").set(dd, "02:00:00:00:00:01");
                ddCls.getField("deviceClass").set(dd, "000200404");
                ddCls.getField("serviceTypes").setInt(dd, 7); // HFP | A2DP | AVRCP
                args[0] = dd;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateTrustedDevices")) {
                Class<?> tdCls = Class.forName("org.dsi.ifc.bluetooth.TrustedDevice", false, cl);
                Object td = tdCls.getDeclaredConstructor().newInstance();
                tdCls.getField("deviceName").set(td, "Demo phone");
                tdCls.getField("deviceAddress").set(td, "02:00:00:00:00:01");
                tdCls.getField("deviceRole").setInt(td, 1); // SLAVE
                tdCls.getField("deviceClass").setInt(td, 0x200404); // Smartphone
                tdCls.getField("deviceSecurity").setInt(td, 1);
                tdCls.getField("linkMode").setInt(td, 1);
                tdCls.getField("linkkeyStrength").setInt(td, 16);
                tdCls.getField("lastConnectedServiceTypes").setInt(td, 7);
                tdCls.getField("activeServiceTypes").setInt(td, 7);
                tdCls.getField("offeredServiceTypes").setInt(td, 7);
                Object tdArr = Array.newInstance(tdCls, 1);
                Array.set(tdArr, 0, td);
                args[0] = tdArr;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateBTState")) {
                args[0] = 1; // BTSTATE_ON
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateReconnectIndicator") && args[0] != null) {
                Object ri = args[0];
                try {
                    ri.getClass().getField("reconnectIndicator").setInt(ri, 0);
                    if (ri.getClass().getField("deviceNameList").get(ri) == null) {
                        ri.getClass().getField("deviceNameList").set(ri, new String[] { "Demo phone" });
                    }
                    if (ri.getClass().getField("serviceTypeList").get(ri) == null) {
                        ri.getClass().getField("serviceTypeList").set(ri, new int[] { 1, 2 });
                    }
                } catch (Throwable ignored) {}
                if (args.length > 1) args[1] = VALID;
                return;
            }

            // Phone
            if (method.equals("updateTopology")) {
                args[0] = new int[] { 0, 1 };
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateUsage")) {
                args[0] = new int[] { 1, 1 };
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateAdbState")) {
                args[0] = 2; // ADB_STATE_READY
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateActivationState") && args[0] != null) {
                Object a = args[0];
                Class<?> c = a.getClass();
                c.getField("telActivationState").setInt(a, 5 /* PHONE_ON */);
                c.getField("telPhoneModuleState").setInt(a, 2 /* PHONEMODULESTATE_ON */);
                c.getField("telMode").setInt(a, 3 /* Bluetooth HFP */);
                try { c.getField("telFeat").setShort(a, (short) 0x7fff); } catch (Throwable ignored) {}
                try { c.getField("telHFPVersion").set(a, "1.6"); } catch (Throwable ignored) {}
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateLockState") && args[0] != null) {
                Object a = args[0];
                try { a.getClass().getField("telLockState").setInt(a, 2 /* LOCKSTATE_NO_LOCK */); } catch (Throwable ignored) {}
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateSignalQuality") || method.equals("updateSignalStrength")) {
                args[0] = 4;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateBatteryChargeLevel") || method.equals("updateBatteryLevel")) {
                args[0] = 85;
                if (args.length > 1) args[1] = VALID;
                return;
            }

            // Radio FM Tuner
            if (method.equals("updateAvailability")) {
                args[0] = 2; // DEVICESTATUS_READY
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateAFSwitchStatus")) {
                args[0] = Boolean.TRUE;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateStationList")) {
                Class<?> stCls = Class.forName("org.dsi.ifc.radio.Station", false, cl);
                Class<?> rlCls = Class.forName("org.dsi.ifc.global.ResourceLocator", false, cl);
                Object stArr = Array.newInstance(stCls, 5);
                int[] freqs = { 87500, 89300, 95800, 101100, 104600 };
                String[] names = { "Radio 1", "Radio 2", "Antenne", "Bayern 3", "Hitradio" };
                for (int k = 0; k < 5; k++) {
                    Object s = stCls.getDeclaredConstructor().newInstance();
                    stCls.getField("name").set(s, names[k]);
                    stCls.getField("realPS").set(s, names[k]);
                    stCls.getField("frequency").setLong(s, freqs[k]);
                    stCls.getField("pi").setInt(s, 0xD311 + k);
                    stCls.getField("receptionQuality").setInt(s, 100);
                    stCls.getField("waveband").setInt(s, 1 /* FM */);
                    stCls.getField("rds").setBoolean(s, true);
                    stCls.getField("radioText").setBoolean(s, true);
                    stCls.getField("stationArt").set(s, rlCls.getDeclaredConstructor().newInstance());
                    Array.set(stArr, k, s);
                }
                args[0] = stArr;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateSelectedStation")) {
                Class<?> stCls = Class.forName("org.dsi.ifc.radio.Station", false, cl);
                Class<?> rlCls = Class.forName("org.dsi.ifc.global.ResourceLocator", false, cl);
                Object s = stCls.getDeclaredConstructor().newInstance();
                stCls.getField("name").set(s, "Radio 1");
                stCls.getField("realPS").set(s, "Radio 1");
                stCls.getField("frequency").setLong(s, 87500L);
                stCls.getField("pi").setInt(s, 0xD311);
                stCls.getField("receptionQuality").setInt(s, 100);
                stCls.getField("waveband").setInt(s, 1);
                stCls.getField("rds").setBoolean(s, true);
                stCls.getField("radioText").setBoolean(s, true);
                stCls.getField("stationArt").set(s, rlCls.getDeclaredConstructor().newInstance());
                args[0] = s;
                if (args.length > 1) args[1] = VALID;
                return;
            }
            if (method.equals("updateRadioText")) {
                Class<?> rtCls = Class.forName("org.dsi.ifc.radio.AMFMRadioText", false, cl);
                Constructor<?> rtCtor = rtCls.getConstructor(int.class, String.class);
                args[0] = rtCtor.newInstance(0xD311, "Volkswagen Golf 7 - Playing Now");
                if (args.length > 1) args[1] = VALID;
                return;
            }

            if (args[0] == null) return;
            Object a = args[0];
            Class<?> c = a.getClass();
            switch (method) {
                case "updateDoorLockingLockStatus":
                    try {
                        c.getField("vehicleLockState").setInt(a, doorsLocked ? 0 : 1);
                        c.getField("driverDoor").setInt(a, doorFrontLeftOpen ? 1 : 0);
                        c.getField("codriverDoor").setInt(a, doorFrontRightOpen ? 1 : 0);
                        c.getField("fondDoorLeft").setInt(a, doorRearLeftOpen ? 1 : 0);
                        c.getField("fondDoorRight").setInt(a, doorRearRightOpen ? 1 : 0);
                        c.getField("hood").setInt(a, hoodOpen ? 1 : 0);
                        c.getField("bootlid").setInt(a, trunkOpen ? 1 : 0);
                        c.getField("flap").setInt(a, 0);
                        c.getField("vehicleSafe").setBoolean(a, doorsLocked);
                    } catch (Throwable ignored) {}
                    break;
                case "updatePhoneInformation":
                    try { c.getField("meBtAddress").set(a, "02:00:00:00:00:01"); } catch (Throwable ignored) {}
                    try { c.getField("meFriendlyName").set(a, "Demo phone"); } catch (Throwable ignored) {}
                    try { c.getField("telNetIdAvailMeBtAddress").setInt(a, 1); } catch (Throwable ignored) {}
                    try { c.getField("telNetIdAvailMeFriendlyName").setInt(a, 1); } catch (Throwable ignored) {}
                    break;
                case "updateNetworkProvider":
                case "updateRegisterState":
                    try { c.getField("telRegisterState").setInt(a, 1); } catch (Throwable ignored) {}
                    try { c.getField("telLongProviderName").set(a, "Demo phone"); } catch (Throwable ignored) {}
                    try { c.getField("telRegMode").setInt(a, 1); } catch (Throwable ignored) {}
                    break;
                case "updateDynamicVehicleInfoHighFrequent":
                    try {
                        c.getField("engineSpeed").setInt(a, (int) vehicleRpm);
                        c.getField("currentOutputPower").setFloat(a, (float) Math.min(162.0, (vehicleRpm / 6000.0) * 162.0));
                        c.getField("absChargingAirPressure").setFloat(a, (float)(1.0 + (vehicleRpm > 2000 ? Math.min(1.4, (vehicleRpm - 2000) / 3000.0 * 1.4) : 0.0)));
                        c.getField("currentTorque").setInt(a, (int) Math.min(350, vehicleRpm > 1500 ? (200 + (vehicleRpm - 1500) / 10) : 100));
                        c.getField("acceleratorPosition").setByte(a, (byte)(vehicleSpeed > 0 ? Math.min(100, (int)(vehicleSpeed / 2.0)) : 0));
                        c.getField("lateralAcceleration").setFloat(a, (float)(vehicleSpeed > 10 ? Math.sin(System.currentTimeMillis() / 2500.0) * 0.25 : 0.0));
                        c.getField("longitudinalAcceleration").setFloat(a, (float)(vehicleSpeed > 5 ? 0.2 : 0.0));
                    } catch (Throwable ignored) {}
                    break;
                case "updateDynamicVehicleInfoMidFrequent":
                    try {
                        c.getField("gearboxOilTemperature").setInt(a, engineRunning ? 88 : 20);
                        c.getField("batteryVoltage").setFloat(a, engineRunning ? 14.2f : 12.4f);
                        c.getField("daytimeRunningLightIsActive").setBoolean(a, true);
                        c.getField("lowBeamHeadlightIsActive").setBoolean(a, true);
                        c.getField("automaticGearShiftTransMode").setInt(a, 1); // Drive
                        int gear = 1;
                        if (vehicleSpeed > 140) gear = 6;
                        else if (vehicleSpeed > 105) gear = 5;
                        else if (vehicleSpeed > 75) gear = 4;
                        else if (vehicleSpeed > 45) gear = 3;
                        else if (vehicleSpeed > 20) gear = 2;
                        c.getField("currentGear").setInt(a, gear);
                        c.getField("recommendedGear").setInt(a, gear);
                    } catch (Throwable ignored) {}
                    break;
                case "updateBCCoolantTemperature":
                    try {
                        c.getField("state").setInt(a, VALID);
                        c.getField("temperatureValue").setFloat(a, 90.0f);
                        c.getField("temperatureUnit").setInt(a, 0); // CELSIUS
                    } catch (Throwable ignored) {}
                    break;
                case "updateBCOilTemperatureValue":
                    try {
                        c.getField("state").setInt(a, VALID);
                        c.getField("temperatureValue").setFloat(a, (float)(engineRunning ? 96.0 + (vehicleSpeed / 25.0) : 22.0));
                        c.getField("temperatureUnit").setInt(a, 0); // CELSIUS
                    } catch (Throwable ignored) {}
                    break;
                case "updateBCOutsideTemperature":
                    try {
                        c.getField("state").setInt(a, VALID);
                        c.getField("temperatureValue").setFloat(a, 19.5f);
                        c.getField("temperatureUnit").setInt(a, 0); // CELSIUS
                    } catch (Throwable ignored) {}
                    break;
                case "updateBCMenue1Config":
                case "updateBCMenue2Config":
                case "updateBCMenue3Config":
                    c.getField("cruisingRange").setBoolean(a, true);
                    c.getField("averageConsumption").setBoolean(a, true);
                    c.getField("currentConsumption").setBoolean(a, true);
                    c.getField("averageSpeed").setBoolean(a, true);
                    c.getField("drivingTime").setBoolean(a, true);
                    c.getField("drivenDistance").setBoolean(a, true);
                    c.getField("menueDisplay").setBoolean(a, true);
                    c.getField("refuelVolume").setBoolean(a, true);
                    c.getField("coolantTemp").setBoolean(a, true);
                    c.getField("zeroEmissionTime").setBoolean(a, false);
                    c.getField("zeroEmissionDistance").setBoolean(a, false);
                    break;
                case "updateBCShortTermAverageConsumption1":
                case "updateBCLongTermAverageConsumption1":
                case "updateBCCycleAverageConsumption1":
                    c.getField("state").setInt(a, VALID);
                    c.getField("consumptionValue").setFloat(a, (float) avgConsumption);
                    c.getField("consumptionUnit").setInt(a, 0);
                    break;
                case "updateBCCurrentRange1":
                    c.getField("state").setInt(a, VALID);
                    c.getField("currentRangeValue").setInt(a, range);
                    c.getField("currentRangeUnit").setInt(a, 0); // km
                    break;
                case "updateBCTotalDistance":
                    c.getField("distanceValueState").setInt(a, VALID);
                    c.getField("distanceValue").setDouble(a, odometer);
                    c.getField("distanceUnit").setInt(a, 0); // km
                    break;
                case "updateBCTankLevel1":
                    c.getField("state").setInt(a, VALID);
                    c.getField("tankLevelValue").setInt(a, fuel); // liters
                    break;
                case "updateBCShortTermGeneral":
                    setGeneralData(a, tripDistance, avgSpeed, tripMinutes);
                    break;
                case "updateBCLongTermGeneral":
                    setGeneralData(a, odometer, avgSpeed, tripMinutes * 10);
                    break;
                case "updateBCCycleGeneral":
                    setGeneralData(a, tripDistance, avgSpeed, tripMinutes);
                    break;
            }
            if (args.length > 1 && args[args.length - 1] instanceof Integer) {
                args[args.length - 1] = Integer.valueOf(VALID);
            }
        } catch (Throwable ignored) {}
    }

    private static void applyClockTelemetry(String method, Object[] args) {
        if (args == null || args.length == 0) return;
        try {
            java.time.LocalDateTime now = java.time.LocalDateTime.now();
            if (method.equals("updateClockTime") && args[0] != null) {
                Object ct = args[0];
                Class<?> c = ct.getClass();
                c.getField("hours").setByte(ct, (byte) now.getHour());
                c.getField("minutes").setByte(ct, (byte) now.getMinute());
                c.getField("seconds").setByte(ct, (byte) now.getSecond());
            } else if (method.equals("updateClockDate") && args[0] != null) {
                Object cd = args[0];
                Class<?> c = cd.getClass();
                c.getField("year").setShort(cd, (short) (now.getYear() - 2000));
                c.getField("month").setByte(cd, (byte) now.getMonthValue());
                c.getField("day").setByte(cd, (byte) now.getDayOfMonth());
            }
        } catch (Throwable ignored) {}
    }

    private static void setGeneralData(Object a, double km, float kmh, int mins) {
        try {
            Class<?> c = a.getClass();
            Object d = c.getField("distance").get(a);
            if (d != null) {
                d.getClass().getField("distanceValueState").setInt(d, VALID);
                d.getClass().getField("distanceValue").setDouble(d, km);
                d.getClass().getField("distanceUnit").setInt(d, 0);
            }
            Object s = c.getField("speed").get(a);
            if (s != null) {
                s.getClass().getField("speedValueState").setInt(s, VALID);
                s.getClass().getField("speedValue").setFloat(s, kmh);
                s.getClass().getField("speedUnit").setInt(s, 0);
            }
            Object t = c.getField("timeValue").get(a);
            if (t != null) {
                t.getClass().getField("state").setInt(t, VALID);
                t.getClass().getField("timeValue").setInt(t, mins);
            }
        } catch (Throwable ignored) {}
    }

    private static void fixFullRangeUpdate(String method, Object[] args) {
        if (!method.endsWith("UpdateInfo") || args == null || args.length == 0 || args[0] == null) return;
        Object info = args[0];
        try {
            Field ac = info.getClass().getField("arrayContent");
            Field se = info.getClass().getField("startElement");
            Field ne = info.getClass().getField("numOfElements");
            Field rc = info.getClass().getField("recordContent");
            int a = ac.getInt(info);
            if (a == 0 || a == 1) {
                ac.setInt(info, 4);  // ONLY_CHANGES
                se.setInt(info, 0);
                ne.setInt(info, -1); // ALL elements
                rc.setInt(info, 0);
            }
        } catch (Throwable ignored) {}
    }
}
