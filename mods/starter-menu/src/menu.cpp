// Blaze Menu (the starter menu): a hand menu in the style of ii's Stupid Menu (Gorilla Tag).
// The layout idea comes from ii's menu; the code is new.
//   Hold Y (left hand)      -> menu on your left hand, tipped like ii's; turn your palm to you
//                              (B and the right hand with "Right Hand Menu")
//   Touch a button with your other index finger to press it
//   < and > on the sides    -> previous / next page
//   Setting rows look like (<<) [TEXT] (>>): touch << or >> to change the number
//   Pages: Movement Mods, Fun Mods, Rig Mods, Items, Settings, Enabled Mods
// Movement only changes your own game. Rig mods (dances), Items and Spawn Item are seen by
// the room, within the limits in AGENTS.md ("Things others see"). Read docs/GUIDE.md before adding.
#include "ymsdk.hpp"
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <ctime>
#include <algorithm>
#include <string>
#include <vector>

using namespace BNM;
using namespace BNM::Structures::Unity;
using BNM::IL2CPP::Il2CppObject;

YEEPS_MOD("Blaze Menu")

// ---- small helpers ---------------------------------------------------------

static std::string TypeName(const BNM::IL2CPP::Il2CppType *t) {
    Class c(t);
    if (!c.IsValid() || !c.GetClass() || !c.GetClass()->name) return "?";
    auto *k = c.GetClass();
    return (k->namespaze && k->namespaze[0]) ? std::string(k->namespaze) + "." + k->name : k->name;
}
// Pick an overload by its exact parameter types, e.g. GetComponent(System.Type).
static MethodBase Overload(Class c, const char *name, std::vector<std::string> types) {
    for (auto &m : c.GetMethods(false)) {
        auto *mi = m.GetInfo();
        if (!mi || !mi->name || strcmp(mi->name, name) != 0 || mi->parameters_count != types.size()) continue;
        bool ok = true;
        for (size_t i = 0; i < types.size() && ok; i++) ok = TypeName(mi->parameters[i]) == types[i];
        if (ok) return m;
    }
    return {};
}

static std::string Lower(const char *s) {
    std::string o;
    for (; s && *s; s++) o += (char)tolower((unsigned char)*s);
    return o;
}
static bool HasWord(const std::string &s, std::initializer_list<const char *> words) {
    for (auto w : words) if (s.find(w) != std::string::npos) return true;
    return false;
}

// Unity objects we keep between frames must be held by a GC handle, or the
// garbage collector can free them under us.
static void *(*p_handleNew)(void *, bool);
static void *(*p_handleTarget)(void *);
static void (*p_handleFree)(void *);
struct Ref {
    void *h = nullptr;
    void set(Il2CppObject *o) { clear(); if (o && p_handleNew) h = p_handleNew(o, false); }
    Il2CppObject *get() const { return h && p_handleTarget ? (Il2CppObject *)p_handleTarget(h) : nullptr; }
    void clear() { if (h && p_handleFree) p_handleFree(h); h = nullptr; }
};

static Color Lerp(Color a, Color b, float t) {
    return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1);
}
static Color Rainbow(float h) {             // hue 0..1 -> bright color
    h = h - floorf(h);
    float r = fabsf(h * 6 - 3) - 1, g = 2 - fabsf(h * 6 - 2), b = 2 - fabsf(h * 6 - 4);
    auto c = [](float v) { return v < 0 ? 0.f : v > 1 ? 1.f : v; };
    return Color(c(r), c(g), c(b), 1);
}

// ---- game lookups, resolved once -------------------------------------------

struct XRDevice { uint64_t id; bool initialized; };
static struct {
    bool ok = false;
    Method<Il2CppObject *> createPrimitive, goTransform, goGetComponent, goAddComponent, compTransform, compGameObject;
    Method<void> goSetActive, destroy, trSetParent, trSetPosition, trSetRotation, trSetLocalPosition,
                 trSetLocalRotation, trSetLocalScale, rbSetVelocity, matSetColor, matSetShader,
                 textSetFont, textSetText, textSetFontSize, textSetAlignment, graphicSetColor,
                 canvasSetRenderMode, rtSetSizeDelta, textSetFontStyle, textSetBestFit, textSetMinSize,
                 textSetMaxSize, teleportTo, rbAddForce, rbSetUseGravity, colliderSetEnabled,
                 lrSetPositionCount, lrSetPosition, lrSetStartWidth, lrSetEndWidth, lrSetUseWorldSpace;
    Method<Quaternion> trGetRotation;
    Method<Vector3> trGetPosition, trGetForward, trGetRight, trGetUp, trInverseTransformPoint, trTransformPoint, rbGetVelocity;
    Method<bool> alive, tryGetBool, haptic, raycast;
    Method<XRDevice> deviceAtNode;
    Method<Il2CppObject *> trGetParent, trGetRoot, getComponentsInChildren, netLocal;
    Method<void> pvRpc;
    BNM::IL2CPP::MethodInfo *rpcCreateItem = nullptr, *rpcDespawn = nullptr;     // only read for its parameter types
    BNM::IL2CPP::Il2CppClass *objectClass = nullptr, *stringClass = nullptr;
    BNM::MonoType *tPhotonView = nullptr, *tItem = nullptr;
    Method<BNM::Structures::Mono::String *> itemKey, itemId, objGetName;
    Method<bool> itemHeld;
    Method<void> itemDrop, setHeldItem;
    void *(*arrayNew)(void *, uintptr_t) = nullptr;
    void *(*valueBox)(void *, void *) = nullptr;
    Method<bool> behaviourGetEnabled;
    Method<void> behaviourSetEnabled;
    std::vector<BNM::MonoType *> trackerTypes;
    Method<Il2CppObject *> rendererGetMaterial, shaderFind, findAll, findScene, findSceneByType, builtinResource;
    Field<Il2CppObject *> leftHand, rightHand, headTransform, bodyCollider, headCollider;
    Field<float> maxJumpSpeed, jumpMultiplier;
    Method<Vector3> plGetVelocity;
    Method<void> plSetVelocity, plNoGravityFor;
    Field<int> locomotionLayers, collisionLayers;
    Method<int> goGetLayer;
    Method<bool> colliderIsTrigger, behaviourGetEnabledCollider;
    Method<void> ignoreLayers;            // a LayerMask: which layers your hands can climb on
    Method<void> goSetLayer;
    BNM::MonoType *tRigidbody = nullptr, *tCollider = nullptr, *tCanvas = nullptr, *tText = nullptr,
                  *tFont = nullptr, *tRenderer = nullptr, *tMeshCollider = nullptr, *tLine = nullptr;
    Ref shader, font, usagePrimary, usageSecondary, usageGrip, usageTrigger;
} G;

static bool Lookups() {
    if (G.ok) return true;
    auto R = ym::api()->resolve;
    p_handleNew = (decltype(p_handleNew))R("il2cpp_gchandle_new");
    p_handleTarget = (decltype(p_handleTarget))R("il2cpp_gchandle_get_target");
    p_handleFree = (decltype(p_handleFree))R("il2cpp_gchandle_free");
    Class go("UnityEngine", "GameObject"), tr("UnityEngine", "Transform"), obj("UnityEngine", "Object"),
          rb("UnityEngine", "Rigidbody"), mat("UnityEngine", "Material"), rend("UnityEngine", "Renderer"),
          sh("UnityEngine", "Shader"), res("UnityEngine", "Resources"), text("UnityEngine.UI", "Text"),
          graphic("UnityEngine.UI", "Graphic"), canvas("UnityEngine", "Canvas"), rt("UnityEngine", "RectTransform"),
          dev("UnityEngine.XR", "InputDevices"), player("", "MasterPlayer", Image("Assembly-CSharp.dll"));
    if (!go || !tr || !player || !dev) { ym::log("starter-menu: game classes missing"); return false; }
    G.createPrimitive = go.GetMethod("CreatePrimitive", 1);
    G.goTransform = go.GetMethod("get_transform", 0);
    G.goGetComponent = Overload(go, "GetComponent", {"System.Type"});
    G.goAddComponent = Overload(go, "AddComponent", {"System.Type"});
    G.goSetActive = go.GetMethod("SetActive", 1);
    Class comp("UnityEngine", "Component");
    G.compTransform = comp.GetMethod("get_transform", 0);
    G.compGameObject = comp.GetMethod("get_gameObject", 0);
    G.destroy = Overload(obj, "Destroy", {"UnityEngine.Object"});
    G.alive = obj.GetMethod("op_Implicit", 1);
    G.objGetName = obj.GetMethod("get_name", 0);
    G.trSetParent = Overload(tr, "SetParent", {"UnityEngine.Transform", "System.Boolean"});
    G.trSetPosition = tr.GetMethod("set_position", 1);
    G.trSetRotation = tr.GetMethod("set_rotation", 1);
    G.trSetLocalPosition = tr.GetMethod("set_localPosition", 1);
    G.trSetLocalRotation = tr.GetMethod("set_localRotation", 1);
    G.trSetLocalScale = tr.GetMethod("set_localScale", 1);
    G.trGetPosition = tr.GetMethod("get_position", 0);
    G.trGetRotation = tr.GetMethod("get_rotation", 0);
    G.trGetForward = tr.GetMethod("get_forward", 0);
    G.trGetRight = tr.GetMethod("get_right", 0);
    G.trGetUp = tr.GetMethod("get_up", 0);
    G.trInverseTransformPoint = Overload(tr, "InverseTransformPoint", {"UnityEngine.Vector3"});
    G.trTransformPoint = Overload(tr, "TransformPoint", {"UnityEngine.Vector3"});
    G.rbSetVelocity = rb.GetMethod("set_velocity", 1);
    G.rbGetVelocity = rb.GetMethod("get_velocity", 0);
    G.rendererGetMaterial = rend.GetMethod("get_material", 0);
    G.matSetColor = Overload(mat, "set_color", {"UnityEngine.Color"});
    G.matSetShader = mat.GetMethod("set_shader", 1);
    G.shaderFind = sh.GetMethod("Find", 1);
    G.findAll = Overload(res, "FindObjectsOfTypeAll", {"System.Type"});
    G.textSetFont = text.GetMethod("set_font", 1);
    G.textSetFontStyle = text.GetMethod("set_fontStyle", 1);
    G.textSetBestFit = text.GetMethod("set_resizeTextForBestFit", 1);
    G.textSetMinSize = text.GetMethod("set_resizeTextMinSize", 1);
    G.textSetMaxSize = text.GetMethod("set_resizeTextMaxSize", 1);
    G.textSetText = text.GetMethod("set_text", 1);
    G.textSetFontSize = text.GetMethod("set_fontSize", 1);
    G.textSetAlignment = text.GetMethod("set_alignment", 1);
    G.graphicSetColor = graphic.GetMethod("set_color", 1);
    G.canvasSetRenderMode = canvas.GetMethod("set_renderMode", 1);
    G.rtSetSizeDelta = rt.GetMethod("set_sizeDelta", 1);
    G.deviceAtNode = dev.GetMethod("GetDeviceAtXRNode", 1);
    G.tryGetBool = dev.GetMethod("TryGetFeatureValue_bool", 3);
    G.haptic = dev.GetMethod("SendHapticImpulse", 4);    // optional: a small buzz on press
    G.leftHand = player.GetField("leftHandTransform");
    G.rightHand = player.GetField("rightHandTransform");
    G.headTransform = player.GetField("headTransform");
    G.bodyCollider = player.GetField("bodyCollider");
    G.headCollider = player.GetField("headCollider");
    G.maxJumpSpeed = player.GetField("maxJumpSpeed");
    G.locomotionLayers = player.GetField("locomotionEnabledLayers");
    G.collisionLayers = player.GetField("collisionEnabledLayers");
    G.goGetLayer = go.GetMethod("get_layer", 0);
    G.ignoreLayers = Class("UnityEngine", "Physics").GetMethod("IgnoreLayerCollision", 3);
    // Yeeps moves your body through these, not through a Rigidbody on the player object
    G.plGetVelocity = player.GetMethod("GetVelocity", 0);
    G.plSetVelocity = player.GetMethod("ApplySetVelocityNoHaptic", 1);
    if (!G.plSetVelocity.IsValid()) G.plSetVelocity = player.GetMethod("ApplySetVelocity", 1);
    G.plNoGravityFor = player.GetMethod("ApplyNoGravityDuration", 1);
    ym::log("starter-menu: velocity %d/%d, no-gravity %d", G.plGetVelocity.IsValid(), G.plSetVelocity.IsValid(),
            G.plNoGravityFor.IsValid());
    G.goSetLayer = go.GetMethod("set_layer", 1);
    G.jumpMultiplier = player.GetField("jumpMultiplier");
    G.teleportTo = player.GetMethod("TeleportTo", 1);
    G.rbAddForce = Overload(rb, "AddForce", {"UnityEngine.Vector3", "UnityEngine.ForceMode"});
    G.rbSetUseGravity = rb.GetMethod("set_useGravity", 1);
    G.colliderSetEnabled = Class("UnityEngine", "Collider").GetMethod("set_enabled", 1);
    G.behaviourGetEnabledCollider = Class("UnityEngine", "Collider").GetMethod("get_enabled", 0);
    G.colliderIsTrigger = Class("UnityEngine", "Collider").GetMethod("get_isTrigger", 0);
    G.raycast = Overload(Class("UnityEngine", "Physics"), "Raycast",
                         {"UnityEngine.Vector3", "UnityEngine.Vector3", "UnityEngine.RaycastHit", "System.Single", "System.Int32"});
    G.findScene = Overload(obj, "FindObjectsOfType", {"System.Type"});      // only when noclip starts
    G.findSceneByType = Overload(obj, "FindObjectsByType", {"System.Type", "UnityEngine.FindObjectsSortMode"});
    G.builtinResource = Overload(res, "GetBuiltinResource", {"System.Type", "System.String"});
    Class line("UnityEngine", "LineRenderer");
    G.tLine = line.GetMonoType();
    G.lrSetPositionCount = line.GetMethod("set_positionCount", 1);
    G.lrSetPosition = line.GetMethod("SetPosition", 2);
    G.lrSetStartWidth = line.GetMethod("set_startWidth", 1);
    G.lrSetEndWidth = line.GetMethod("set_endWidth", 1);
    G.lrSetUseWorldSpace = line.GetMethod("set_useWorldSpace", 1);
    G.tMeshCollider = Class("UnityEngine", "MeshCollider").GetMonoType();
    G.trGetParent = tr.GetMethod("get_parent", 0);
    G.trGetRoot = tr.GetMethod("get_root", 0);
    G.getComponentsInChildren = Overload(go, "GetComponentsInChildren", {"System.Type", "System.Boolean"});
    Class net("", "NetworkMessenger", Image("Assembly-CSharp.dll"));
    if (net.IsValid()) {
        G.netLocal = net.GetMethod("get_local", 0);
        for (auto &m : net.GetMethods(false)) {
            auto *mi = m.GetInfo();
            if (!mi || !mi->name) continue;
            if (strcmp(mi->name, "RpcCreateItem") == 0) G.rpcCreateItem = mi;
            else if (!G.rpcDespawn && strncmp(mi->name, "Rpc", 3) == 0) {     // the game's "remove an item" message, if it has one
                std::string n = Lower(mi->name);
                if (HasWord(n, {"destroy", "delete", "remove", "despawn"}) && n.find("item") != std::string::npos) G.rpcDespawn = mi;
            }
        }
    }
    {
        Class pv("Photon.Pun", "PhotonView");
        G.tPhotonView = pv.IsValid() ? pv.GetMonoType() : nullptr;
        if (pv.IsValid()) G.pvRpc = Overload(pv, "RPC", {"System.String", "Photon.Pun.RpcTarget", "System.Object[]"});
        Class item("", "Item", Image("Assembly-CSharp.dll"));
        if (item.IsValid()) {
            G.tItem = item.GetMonoType();
            G.itemKey = item.GetMethod("get_itemKey", 0);
            G.itemHeld = item.GetMethod("IsHeldOrAnchoredToAnyPlayer", 0);
            G.itemId = item.GetMethod("get_itemID", 0);
            G.itemDrop = item.GetMethod("TrySendDrop", 0);
            G.setHeldItem = Class("", "MasterPlayer", Image("Assembly-CSharp.dll")).GetMethod("SetHeldItem", 2);
        }
        G.objectClass = Class("System", "Object").GetClass();
        G.stringClass = Class("System", "String").GetClass();
        G.arrayNew = (decltype(G.arrayNew))ym::api()->resolve("il2cpp_array_new");
        G.valueBox = (decltype(G.valueBox))ym::api()->resolve("il2cpp_value_box");
        ym::log("starter-menu: items rpc=%d pv=%d item=%d box=%d", G.rpcCreateItem != nullptr, G.pvRpc.IsValid(),
                G.itemKey.IsValid(), G.arrayNew && G.valueBox);
    }
    Class behaviour("UnityEngine", "Behaviour");
    G.behaviourGetEnabled = behaviour.GetMethod("get_enabled", 0);
    G.behaviourSetEnabled = behaviour.GetMethod("set_enabled", 1);
    // What moves your hands with the controllers (paused while you dance)
    for (auto nc : std::vector<std::pair<const char *, const char *>>{
             {"UnityEngine.XR.Interaction.Toolkit", "XRBaseController"},
             {"UnityEngine.InputSystem.XR", "TrackedPoseDriver"},
             {"UnityEngine.SpatialTracking", "TrackedPoseDriver"}}) {
        Class c(nc.first, nc.second);
        if (c.IsValid()) G.trackerTypes.push_back(c.GetMonoType());
    }
    G.tRigidbody = rb.GetMonoType();
    G.tCollider = Class("UnityEngine", "Collider").GetMonoType();
    G.tRenderer = rend.GetMonoType();
    G.tCanvas = canvas.GetMonoType();
    G.tText = text.GetMonoType();
    G.tFont = Class("UnityEngine", "Font").GetMonoType();
    G.usagePrimary.set((Il2CppObject *)CreateMonoString("PrimaryButton"));
    G.usageSecondary.set((Il2CppObject *)CreateMonoString("SecondaryButton"));
    G.usageGrip.set((Il2CppObject *)CreateMonoString("GripButton"));
    G.usageTrigger.set((Il2CppObject *)CreateMonoString("TriggerButton"));
    for (const char *s : {"Universal Render Pipeline/Unlit", "Universal Render Pipeline/Lit"})
        if (!G.shader.get() && G.shaderFind.IsValid()) G.shader.set(G.shaderFind((BNM::Structures::Mono::String *)CreateMonoString(s)));
    // Arial, like ii's menu (Unity 6 calls it LegacyRuntime.ttf)
    for (const char *f : {"LegacyRuntime.ttf", "Arial.ttf"})
        if (!G.font.get() && G.builtinResource.IsValid() && G.tFont)
            G.font.set(G.builtinResource((Il2CppObject *)G.tFont, CreateMonoString(f)));
    if (!G.font.get() && G.findAll.IsValid() && G.tFont) {   // else any font the game loaded (one time)
        auto *arr = (uint8_t *)G.findAll((Il2CppObject *)G.tFont);
        if (arr && *(uintptr_t *)(arr + 0x18) > 0) G.font.set(((Il2CppObject **)(arr + 0x20))[0]);
    }
    G.ok = G.createPrimitive.IsValid() && G.goTransform.IsValid() && G.trSetPosition.IsValid() &&
           G.deviceAtNode.IsValid() && G.tryGetBool.IsValid() && G.leftHand.IsValid();
    if (!G.ok) ym::log("starter-menu: some game methods are missing, menu off");
    return G.ok;
}

static bool Alive(Il2CppObject *o) { return o && G.alive((Il2CppObject *)o); }
static Il2CppObject *Tr(Il2CppObject *gameObject) { return G.goTransform[gameObject](); }
static void Show(Ref &r, bool on) { if (Alive(r.get())) G.goSetActive[r.get()](on); }
static void Kill(Ref &r) { if (Alive(r.get())) G.destroy(r.get()); r.clear(); }

// A primitive with a flat color. collider=false removes its collider so it can't push you.
// Always give new objects the URP shader: Unity's default material only draws in one
// eye on Quest.
static Il2CppObject *Prim(int type, Color c, bool collider) {
    Il2CppObject *o = G.createPrimitive(type);   // 0 = Sphere, 3 = Cube
    if (!o) return nullptr;
    if (!collider) if (auto *col = G.goGetComponent[o](G.tCollider)) G.destroy(col);
    if (auto *r = G.goGetComponent[o](G.tRenderer)) {
        Il2CppObject *m = G.rendererGetMaterial[r]();
        if (m && G.shader.get()) G.matSetShader[m](G.shader.get());
        if (m) G.matSetColor[m](c);
    }
    return o;
}
static Il2CppObject *Cube(Color c, bool collider) { return Prim(3, c, collider); }
static void SetColor(Il2CppObject *o, Color c) {
    if (!Alive(o)) return;
    if (auto *r = G.goGetComponent[o](G.tRenderer)) if (Il2CppObject *m = G.rendererGetMaterial[r]()) G.matSetColor[m](c);
}
static void SetText(Ref &text, const char *s) {
    if (Alive(text.get())) G.textSetText[text.get()](CreateMonoString(s));
}

// ---- input -------------------------------------------------------------------

struct Buttons { bool a = false, b = false, x = false, y = false, lgrip = false, rgrip = false, ltrig = false, rtrig = false; };
static bool Pressed(int node, Ref &usage) {
    XRDevice d = G.deviceAtNode(node);
    bool v = false;
    if (d.initialized) G.tryGetBool(d.id, (BNM::Structures::Mono::String *)usage.get(), &v);
    return v;
}
static Buttons ReadButtons() {
    const int left = 4, right = 5;              // XRNode.LeftHand / RightHand
    Buttons b;
    b.x = Pressed(left, G.usagePrimary);  b.y = Pressed(left, G.usageSecondary);
    b.a = Pressed(right, G.usagePrimary); b.b = Pressed(right, G.usageSecondary);
    b.lgrip = Pressed(left, G.usageGrip); b.rgrip = Pressed(right, G.usageGrip);
    b.ltrig = Pressed(left, G.usageTrigger); b.rtrig = Pressed(right, G.usageTrigger);
    return b;
}

// ---- menu contents -------------------------------------------------------------

// Feature switches. Every feature only changes your own game.
static bool f_velocityFly, f_tpGun, f_speedBoost, f_airplane, f_longArms, f_boundary, f_outerBoundary,
            f_punch, f_creative, f_farBuild, f_furthestBuild;
static bool f_itemGun, f_spawnOnHand;
static char g_itemKey[64] = "";                // the last item you held (learned on grab)
struct PendingItem { Vector3 pos, vel; bool track = false; };
static std::vector<std::string> g_spawned, g_despawnQueue;   // ids made by Spawn Item / waiting to be removed
static bool SendDespawn(const char *id);
static std::vector<PendingItem> g_itemQueue;
static double g_nextItem = 0, g_nextBatch = 0;
static int g_dupeArmed = 0;

static constexpr int kDances = 8;
static bool g_danceSel[kDances];
static int g_dance = -1;            // the picked dance, -1 = none
static bool f_fly, f_platforms, f_rainbow, f_fps, f_clock, f_rightHand,
            f_notify = true;
static Ref g_player, g_rigidbody, g_leftHand, g_rightHand, g_head;
static Ref g_airL, g_airR, g_spotCube;      // Airplane Fly circles, Save Position cube
static Vector3 g_spot;
static bool g_haveSpot = false;

static void Notify(const char *fmt, ...);
static void SaveSpot();
static void GoToSpot();
static void NextTheme();
static void AllOff();
static void PointerToHand();
static void NextPointer();
static void ApplySpeedBoost();
static void SpeedBoostToggled();
static void LongArmsToggled();
static void PunchToggled();
static void CreativeToggled();
static void FarBuildToggled();
static void FurthestBuildToggled();
static void SpawnHeldItem();
static void DespawnItems();
static void DancePicked();
static void ArmDupe9();
static void ArmDupe27();
static bool LeftHandItem();

enum Cat { MAIN, MOVEMENT, FUN, RIG, ITEMS, SETTINGS, ENABLED, NCAT };
static const char *kCatName[NCAT] = {"Main", "Movement Mods", "Fun Mods", "Rig Mods", "Items", "Settings", "Enabled Mods"};
enum Kind { OPEN, TOGGLE, ACTION, STEPPER };
struct Item {
    Cat cat;
    const char *label;
    Kind kind;
    Cat target = MAIN;            // OPEN: the page it opens
    bool *flag = nullptr;         // TOGGLE: the switch
    void (*fn)() = nullptr;       // ACTION, or called after a TOGGLE / STEPPER changes
    int *val = nullptr;           // STEPPER: the number, shown as (<<) [Label: n] (>>)
    int lo = 0, hi = 0, step = 1; // STEPPER: smallest, biggest, change per press
};
// The owner's layout. Keep this order.
static int g_flySpeed = 10, g_airSpeed = 10, g_boostSpeed = 10;
static Item g_items[] = {
    {MAIN, "Settings", OPEN, SETTINGS},
    {MAIN, "Movement Mods", OPEN, MOVEMENT},
    {MAIN, "Fun Mods", OPEN, FUN},
    {MAIN, "Rig Mods", OPEN, RIG},
    {MAIN, "Items", OPEN, ITEMS},
    {MAIN, "Enabled Mods", OPEN, ENABLED},

    {MOVEMENT, "Exit Movement Mods", OPEN, MAIN},
    {MOVEMENT, "Fly", TOGGLE, MAIN, &f_fly},
    {MOVEMENT, "Fly Speed", STEPPER, MAIN, nullptr, nullptr, &g_flySpeed, 10, 300, 10},
    {MOVEMENT, "Velocity Fly", TOGGLE, MAIN, &f_velocityFly},
    {MOVEMENT, "Airplane Fly", TOGGLE, MAIN, &f_airplane},
    {MOVEMENT, "Plane Speed", STEPPER, MAIN, nullptr, nullptr, &g_airSpeed, 10, 300, 10},
    {MOVEMENT, "Platforms", TOGGLE, MAIN, &f_platforms},
    {MOVEMENT, "Speed Boost", TOGGLE, MAIN, &f_speedBoost, SpeedBoostToggled},
    {MOVEMENT, "Boost Speed", STEPPER, MAIN, nullptr, nullptr, &g_boostSpeed, -9, 30, 1},
    {MOVEMENT, "Long Arms", TOGGLE, MAIN, &f_longArms, LongArmsToggled},
    {MOVEMENT, "Teleport Gun", TOGGLE, MAIN, &f_tpGun},
    {MOVEMENT, "Boundary Pass", TOGGLE, MAIN, &f_boundary},
    {MOVEMENT, "Outer Boundary Pass", TOGGLE, MAIN, &f_outerBoundary},
    {MOVEMENT, "Save Position", ACTION, MAIN, nullptr, SaveSpot},
    {MOVEMENT, "Go To Saved Position", ACTION, MAIN, nullptr, GoToSpot},

    {FUN, "Exit Fun Mods", OPEN, MAIN},
    {FUN, "Punch", TOGGLE, MAIN, &f_punch, PunchToggled},
    {FUN, "Creative", TOGGLE, MAIN, &f_creative, CreativeToggled},
    {FUN, "Farther Build", TOGGLE, MAIN, &f_farBuild, FarBuildToggled},
    {FUN, "Furthest Build", TOGGLE, MAIN, &f_furthestBuild, FurthestBuildToggled},
    {FUN, "Spawn Item", ACTION, MAIN, nullptr, SpawnHeldItem},
    {FUN, "Despawn Items", ACTION, MAIN, nullptr, DespawnItems},

    {RIG, "Exit Rig Mods", OPEN, MAIN},
    {RIG, "Wave", TOGGLE, MAIN, &g_danceSel[0], DancePicked},
    {RIG, "Helicopter", TOGGLE, MAIN, &g_danceSel[1], DancePicked},
    {RIG, "T-Pose", TOGGLE, MAIN, &g_danceSel[2], DancePicked},
    {RIG, "Windmill", TOGGLE, MAIN, &g_danceSel[3], DancePicked},
    {RIG, "Swim", TOGGLE, MAIN, &g_danceSel[4], DancePicked},
    {RIG, "Jumping Jacks", TOGGLE, MAIN, &g_danceSel[5], DancePicked},
    {RIG, "Spaz", TOGGLE, MAIN, &g_danceSel[6], DancePicked},
    {RIG, "Beyblade", TOGGLE, MAIN, &g_danceSel[7], DancePicked},

    {ITEMS, "Exit Items", OPEN, MAIN},
    {ITEMS, "Spawn Item Gun", TOGGLE, MAIN, &f_itemGun},
    {ITEMS, "Duplicate Items (3x3)", ACTION, MAIN, nullptr, ArmDupe9},
    {ITEMS, "Duplicate Items (3x3x3)", ACTION, MAIN, nullptr, ArmDupe27},
    {ITEMS, "Spawn On Hand", TOGGLE, MAIN, &f_spawnOnHand},

    {SETTINGS, "Exit Settings", OPEN, MAIN},
    {SETTINGS, "Change Pointer Position", ACTION, MAIN, nullptr, NextPointer},
    {SETTINGS, "Right Hand Menu", TOGGLE, MAIN, &f_rightHand, PointerToHand},
    {SETTINGS, "Change Menu Theme", ACTION, MAIN, nullptr, NextTheme},
    {SETTINGS, "Rainbow Menu", TOGGLE, MAIN, &f_rainbow},
    {SETTINGS, "Show FPS", TOGGLE, MAIN, &f_fps},
    {SETTINGS, "Show Clock", TOGGLE, MAIN, &f_clock},
    {SETTINGS, "Notifications", TOGGLE, MAIN, &f_notify},
    {SETTINGS, "Turn Everything Off", ACTION, MAIN, nullptr, AllOff},
};
static constexpr int kItems = sizeof(g_items) / sizeof(g_items[0]);
static Item g_returnItem = {ENABLED, "Exit Enabled Mods", OPEN, MAIN};

// Tooltips, shown under the menu while your finger is on a button (like ii's).
static const struct { const char *label, *tip; } kTips[] = {
    {"Settings", "Opens the settings."},
    {"Movement Mods", "Opens the movement mods."},
    {"Fun Mods", "Opens the fun mods."},
    {"Enabled Mods", "Shows every mod you have on."},
    {"Fly", "Hold the right trigger to fly where you look."},
    {"Fly Speed", "<< slower, >> faster (10 each press)."},
    {"Velocity Fly", "Hold the right trigger: you fly where your right hand points, faster the longer you hold. Resets when you let go."},
    {"Airplane Fly", "Hold both hands in the two circles (T-pose) to fly where you look."},
    {"Plane Speed", "<< slower, >> faster (10 each press)."},
    {"Platforms", "Hold a grip to stand on a platform."},
    {"Speed Boost", "Passive: you move faster (or slower with a negative number)."},
    {"Boost Speed", "<< lower, >> higher (1 each press). Below 0 slows you down."},
    {"Long Arms", "Your arms reach farther. They go normal while the menu is open."},
    {"Teleport Gun", "Hold right grip to aim, trigger to teleport."},
    {"Boundary Pass", "Lets you go past the invisible boundary around the map."},
    {"Outer Boundary Pass", "Lets you go past the outer invisible boundary."},
    {"Save Position", "Saves where you are and puts a cube there."},
    {"Go To Saved Position", "Teleports you to your saved position."},
    {"Punch", "Lets other players launch you with a punch."},
    {"Creative", "In play mode, lets you place creative blocks."},
    {"Farther Build", "Build with normal blocks in the outer terrain."},
    {"Furthest Build", "Build past the terrain."},
    {"Spawn Item", "Spawns the item in your left hand at your position."},
    {"Despawn Items", "Removes every item you spawned with Spawn Item."},
    {"Change Menu Theme", "Blaze, Orange, Blue, Green, Purple, Dark."},
    {"Rainbow Menu", "The menu cycles through colors."},
    {"Show FPS", "Frames per second in the title."},
    {"Show Clock", "The time in the title."},
    {"Rig Mods", "Pick a dance, then hold left trigger. Everyone sees it."},
    {"Items", "Spawn copies of the item in your left hand. Everyone sees them."},
    {"Spawn Item Gun", "Hold an item in your left hand. Right grip aims, trigger spawns it."},
    {"Duplicate Items (3x3)", "9 of the item in your left hand appear in front of you."},
    {"Duplicate Items (3x3x3)", "27 of the item in your left hand appear in front of you."},
    {"Spawn On Hand", "Each right trigger spawns the item from your left hand at your right hand."},
    {"Wave", "Waves one hand, the other on your hip. Hold left trigger."},
    {"Helicopter", "Both arms spin around you. Hold left trigger."},
    {"T-Pose", "The classic T-pose. Hold left trigger."},
    {"Windmill", "Arms spin like a windmill. Hold left trigger."},
    {"Swim", "Swims through the air. Hold left trigger."},
    {"Jumping Jacks", "Arms go up and down at your sides. Hold left trigger."},
    {"Spaz", "Hands shake all over the place. Hold left trigger."},
    {"Beyblade", "Hands spin fast around your waist. Hold left trigger."},
    {"Change Pointer Position", "Moves the ball on your finger."},
    {"Right Hand Menu", "Hold B, menu on your right hand."},
    {"Notifications", "Messages when you switch something."},
    {"Turn Everything Off", "Switches every mod off."},
};
static const char *TipFor(const char *label) {
    if (strncmp(label, "Exit ", 5) == 0) return "Returns you back to the main category.";
    for (auto &t : kTips) if (strcmp(t.label, label) == 0) return t.tip;
    return "";
}

// Colors in the ii menu style: an animated background and darker buttons when on.
struct Theme { const char *name; Color bgA, bgB, button, buttonOn, title; };
static const Theme kThemes[] = {
    {"Blaze",  Color(0.90f, 0.12f, 0.04f, 1), Color(1.0f, 0.42f, 0.0f, 1), Color(0.50f, 0.06f, 0.03f, 1), Color(1.0f, 0.45f, 0.05f, 1), Color(1.0f, 0.62f, 0.20f, 1)},
    {"Orange", Color(1.0f, 0.50f, 0.0f, 1), Color(1.0f, 0.40f, 0.0f, 1), Color(0.67f, 0.33f, 0.0f, 1), Color(0.33f, 0.16f, 0.0f, 1), Color(1.0f, 0.75f, 0.49f, 1)},
    {"Blue",   Color(0.20f, 0.50f, 1.0f, 1), Color(0.10f, 0.30f, 0.85f, 1), Color(0.10f, 0.28f, 0.65f, 1), Color(0.04f, 0.12f, 0.32f, 1), Color(0.70f, 0.85f, 1.0f, 1)},
    {"Green",  Color(0.30f, 0.85f, 0.30f, 1), Color(0.15f, 0.65f, 0.20f, 1), Color(0.12f, 0.45f, 0.12f, 1), Color(0.05f, 0.22f, 0.05f, 1), Color(0.75f, 1.0f, 0.75f, 1)},
    {"Purple", Color(0.65f, 0.30f, 1.0f, 1), Color(0.45f, 0.15f, 0.85f, 1), Color(0.38f, 0.15f, 0.60f, 1), Color(0.18f, 0.06f, 0.30f, 1), Color(0.88f, 0.75f, 1.0f, 1)},
    {"Dark",   Color(0.12f, 0.12f, 0.14f, 1), Color(0.05f, 0.05f, 0.06f, 1), Color(0.22f, 0.22f, 0.25f, 1), Color(0.45f, 0.45f, 0.50f, 1), Color(0.85f, 0.85f, 0.90f, 1)},
};
static constexpr int kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);
static int g_theme = 0;
static const Theme &T() { return kThemes[g_theme]; }

// ---- menu objects ----------------------------------------------------------------

// Layout in meters, matching ii's menu as it was ported to Yeeps. Everything is drawn
// on a "board" object: +x goes across, +y goes up, the player looks at it from -z.
static constexpr int kRows = 8;                                   // buttons per page
static const float kBoardW = 0.30f, kBoardH = 0.3825f, kBtnW = 0.27f, kBtnH = 0.0306f;
static const float kTitleY = 0.235f, kSideX = 0.2f, kSideW = 0.06f, kSideH = 0.344f;
static const float kStepW = 0.05f, kStepGap = 0.004f;                  // the << and >> buttons of a setting row
static float StepX() { return kBtnW / 2 - kStepW / 2; }
static float RowY(int r) { return 0.13f - r * 0.04f; }

static Ref g_menu, g_board, g_bg, g_outline, g_pointer, g_title, g_titleBar;
static Ref g_rowCube[kRows], g_rowLabel[kRows], g_rowText[kRows];
static Ref g_rowL[kRows], g_rowR[kRows], g_rowLT[kRows], g_rowRT[kRows];   // << and >> (setting rows)
static int g_hitZone = 0;                                         // 0 middle, 1 <<, 2 >> of the touched row
static Ref g_prev, g_next, g_notifyGo, g_notifyText, g_tipText, g_leftPlatform, g_rightPlatform;
static double g_openTime = 0, g_flashUntil = 0;
static int g_flashRow = -2, g_hover = -2;
static bool g_built = false, g_menuShown = false, g_dirty = true;
static Cat g_cat = MAIN;
static int g_page = 0;
static std::vector<Item *> g_view;                                // what the current page list shows
static double g_time = 0, g_nextPress = 0, g_notifyUntil = 0, g_nextTitle = 0;
static int g_touching = -2, g_frames = 0;
static float g_fps = 0;

static int PageCount() { return g_view.empty() ? 1 : (int)(g_view.size() + kRows - 1) / kRows; }

static void BuildView() {
    g_view.clear();
    if (g_cat == ENABLED) {
        g_view.push_back(&g_returnItem);
        for (auto &it : g_items) if (it.kind == TOGGLE && *it.flag && it.flag != &f_notify) g_view.push_back(&it);
    } else {
        for (auto &it : g_items) if (it.cat == g_cat) g_view.push_back(&it);
    }
    if (g_page >= PageCount()) g_page = 0;
}

static Il2CppObject *Label(Il2CppObject *canvasT, const char *s, Vector3 pos, float w, float h, int size, Ref *textOut) {
    if (!G.font.get() || !G.tText) return nullptr;
    Il2CppObject *go = Class("UnityEngine", "GameObject").CreateNewObjectParameters(CreateMonoString("Label"));
    Il2CppObject *text = go ? G.goAddComponent[go](G.tText) : nullptr;
    if (!text) return nullptr;
    Il2CppObject *t = Tr(go);
    G.trSetParent[t](canvasT, false);
    G.rtSetSizeDelta[t](Vector2(w * 1000, h * 1000));
    G.trSetLocalPosition[t](pos * 1000);
    G.trSetLocalRotation[t](Quaternion(0, 0, 0, 1));
    G.trSetLocalScale[t](Vector3(1, 1, 1));
    G.textSetFont[text](G.font.get());
    G.textSetText[text](CreateMonoString(s));
    G.textSetFontSize[text](size);
    G.textSetAlignment[text](4);                 // MiddleCenter
    if (G.textSetFontStyle.IsValid()) G.textSetFontStyle[text](2);   // Italic, like ii's menu
    if (G.textSetBestFit.IsValid()) {            // shrink long labels to fit their button
        G.textSetBestFit[text](true);
        G.textSetMinSize[text](1);
        G.textSetMaxSize[text](size);
    }
    G.graphicSetColor[text](Color(1, 1, 1, 1));
    if (textOut) textOut->set(text);
    return go;
}

// A world-space canvas (1 unit = 1 mm) under parent.
static Il2CppObject *Canvas(Il2CppObject *parentT, Vector3 pos, float w, float h) {
    Il2CppObject *go = Class("UnityEngine", "GameObject").CreateNewObjectParameters(CreateMonoString("StarterMenuText"));
    Il2CppObject *canvas = go && G.tCanvas ? G.goAddComponent[go](G.tCanvas) : nullptr;
    if (!canvas) return nullptr;
    G.canvasSetRenderMode[canvas](2);            // WorldSpace
    Il2CppObject *ct = Tr(go);
    G.trSetParent[ct](parentT, false);
    G.rtSetSizeDelta[ct](Vector2(w * 1000, h * 1000));
    G.trSetLocalPosition[ct](pos);
    G.trSetLocalRotation[ct](Quaternion(0, 0, 0, 1));
    G.trSetLocalScale[ct](Vector3(0.001f, 0.001f, 0.001f));
    return go;
}

static Il2CppObject *Part(Il2CppObject *parentT, Color c, Vector3 pos, Vector3 scale) {
    Il2CppObject *o = Cube(c, false);
    if (!o) return nullptr;
    Il2CppObject *t = Tr(o);
    G.trSetParent[t](parentT, false);
    G.trSetLocalPosition[t](pos);
    G.trSetLocalScale[t](scale);
    return o;
}

// Where the ball sits on your hand (ii's four pointer positions).
static const Vector3 kPointerSpots[] = {Vector3(0.01f, -0.017f, 0.19f), Vector3(0, -0.1f, 0.05f),
                                        Vector3(0, 0.1f, -0.05f), Vector3(0, 0.0666f, 0.1f)};
static int g_pointerSpot = 0;

static void PointerToHand() {
    Il2CppObject *p = g_pointer.get(), *hand = f_rightHand ? g_leftHand.get() : g_rightHand.get();
    if (!Alive(p) || !hand) return;
    Il2CppObject *t = Tr(p);
    Vector3 at = kPointerSpots[g_pointerSpot];
    if (f_rightHand) at.x = -at.x;
    G.trSetParent[t](hand, false);
    G.trSetLocalPosition[t](at);
}
static void NextPointer() { g_pointerSpot = (g_pointerSpot + 1) % 4; PointerToHand(); Notify("Pointer position %d", g_pointerSpot + 1); }

// Built once per room; afterwards only shown/hidden, moved and relabeled.
static void BuildMenu() {
    auto NewObject = [](const char *name) {
        return Class("UnityEngine", "GameObject").CreateNewObjectParameters(CreateMonoString(name));
    };
    Il2CppObject *menu = NewObject("BlazeMenu"), *board = NewObject("BlazeMenuBoard");
    if (!menu || !board) return;
    g_menu.set(menu);
    g_board.set(board);
    Il2CppObject *bt = Tr(board);
    G.trSetParent[bt](Tr(menu), false);
    // Like ii's menu: the board stands 10 cm out from the hand on its +x side, facing +x.
    G.trSetLocalPosition[bt](Vector3(0.1f, 0, 0));
    G.trSetLocalRotation[bt](Quaternion::LookRotation(Vector3(-1, 0, 0), Vector3(0, 0, 1)));

    g_outline.set(Part(bt, T().title, Vector3(0, 0, 0.004f), Vector3(kBoardW + 0.008f, kBoardH + 0.008f, 0.004f)));
    g_bg.set(Part(bt, T().bgA, Vector3(0, 0, 0), Vector3(kBoardW, kBoardH, 0.01f)));
    for (int r = 0; r < kRows; r++)
        g_rowCube[r].set(Part(bt, T().button, Vector3(0, RowY(r), -0.005f), Vector3(kBtnW, kBtnH, 0.009f)));
    for (int r = 0; r < kRows; r++) {
        g_rowL[r].set(Part(bt, T().buttonOn, Vector3(-StepX(), RowY(r), -0.005f), Vector3(kStepW, kBtnH, 0.009f)));
        g_rowR[r].set(Part(bt, T().buttonOn, Vector3(StepX(), RowY(r), -0.005f), Vector3(kStepW, kBtnH, 0.009f)));
    }
    g_prev.set(Part(bt, T().button, Vector3(-kSideX, 0, 0), Vector3(kSideW, kSideH, 0.009f)));
    // The title sits on its own dark bar above the board (where ii's Disconnect button was)
    g_titleBar.set(Part(bt, T().buttonOn, Vector3(0, kTitleY, -0.002f), Vector3(kBtnW, 0.045f, 0.009f)));
    g_next.set(Part(bt, T().button, Vector3(kSideX, 0, 0), Vector3(kSideW, kSideH, 0.009f)));

    if (Il2CppObject *canvas = Canvas(bt, Vector3(0, 0, -0.0105f), kBoardW + 2 * kSideX, kBoardH + 0.1f)) {
        Il2CppObject *ct = Tr(canvas);
        Label(ct, "Blaze", Vector3(0, kTitleY, 0), kBtnW, 0.045f, 32, &g_title);
        for (int r = 0; r < kRows; r++) {
            g_rowLabel[r].set(Label(ct, "", Vector3(0, RowY(r), 0), kBtnW, kBtnH, 19, &g_rowText[r]));
            g_rowLT[r].set(Label(ct, "<<", Vector3(-StepX(), RowY(r), 0), kStepW, kBtnH, 19, nullptr));
            g_rowRT[r].set(Label(ct, ">>", Vector3(StepX(), RowY(r), 0), kStepW, kBtnH, 19, nullptr));
        }
        Label(ct, "<", Vector3(-kSideX, 0, -0.0005f), kSideW, 0.05f, 34, nullptr);
        Label(ct, ">", Vector3(kSideX, 0, -0.0005f), kSideW, 0.05f, 34, nullptr);
        Label(ct, "", Vector3(0, -kBoardH / 2 - 0.02f, 0), kBoardW + 0.1f, 0.03f, 15, &g_tipText);
        char ver[64];
        snprintf(ver, sizeof ver, "Blaze build %s", ym::api()->gameVersion);
        Label(ct, ver, Vector3(0, -kBoardH / 2 + 0.012f, 0), kBoardW, 0.02f, 11, nullptr);
    }
    // Pointer: a small ball on the index finger of the other hand
    if (Il2CppObject *p = Prim(0, T().bgB, false)) {
        G.trSetLocalScale[Tr(p)](Vector3(0.01f, 0.01f, 0.01f));
        g_pointer.set(p);
        PointerToHand();
    }
    // Notifications: a line of text low in your view, like ii's notifications
    if (Il2CppObject *n = Canvas(g_head.get(), Vector3(0, -0.12f, 0.6f), 0.5f, 0.05f)) {
        g_notifyGo.set(n);
        Label(Tr(n), "", Vector3(0, 0, 0), 0.5f, 0.05f, 22, &g_notifyText);
        G.goSetActive[n](false);
    }
    G.goSetActive[menu](false);
    Show(g_pointer, false);
    g_built = true;
    g_dirty = true;
}

static void Notify(const char *fmt, ...) {
    if (!f_notify || !Alive(g_notifyGo.get())) return;
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    SetText(g_notifyText, buf);
    G.goSetActive[g_notifyGo.get()](true);
    g_notifyUntil = g_time + 2.0;
}

// Relabels and recolors the rows. Only runs after something changed.
static void RenderPage() {
    BuildView();
    for (int r = 0; r < kRows; r++) {
        int i = g_page * kRows + r;
        bool used = i < (int)g_view.size();
        Show(g_rowCube[r], used);
        Show(g_rowLabel[r], used);
        bool step = used && g_view[i]->kind == STEPPER;
        for (Ref *x : {&g_rowL[r], &g_rowR[r], &g_rowLT[r], &g_rowRT[r]}) Show(*x, step);
        if (!used) continue;
        Item *it = g_view[i];
        bool on = it->kind == TOGGLE && *it->flag;
        if (Alive(g_rowCube[r].get()))      // a setting row's middle is narrower, to make room for << and >>
            G.trSetLocalScale[Tr(g_rowCube[r].get())](Vector3(step ? kBtnW - 2 * (kStepW + kStepGap) : kBtnW, kBtnH, 0.009f));
        if (step) {
            char buf[64];
            snprintf(buf, sizeof buf, "%s: %d", it->label, *it->val);
            SetText(g_rowText[r], buf);
        } else SetText(g_rowText[r], it->label);
        SetColor(g_rowCube[r].get(), r == g_flashRow ? Color(1, 0.9f, 0.6f, 1) : on ? T().buttonOn : T().button);
        if (Alive(g_rowText[r].get())) G.graphicSetColor[g_rowText[r].get()](on ? Color(1, 1, 1, 1) : T().title);
    }
    bool pages = PageCount() > 1;
    Show(g_prev, pages);
    Show(g_next, pages);
    SetColor(g_prev.get(), T().button);
    SetColor(g_next.get(), T().button);
    SetColor(g_outline.get(), T().title);
    SetColor(g_titleBar.get(), T().buttonOn);
    SetColor(g_pointer.get(), T().bgB);
    if (Alive(g_title.get())) G.graphicSetColor[g_title.get()](Color(1, 1, 1, 1));
    g_nextTitle = 0;                     // refresh the title now
    g_dirty = false;
}

static int Hex(float v) { return (int)(v < 0 ? 0 : v > 1 ? 255 : v * 255); }

// The title is "Blaze" with a moving red-orange-yellow gradient, one color per letter
// (rich text). Rebuilt ten times a second, not every frame.
static void UpdateTitle() {
    if (g_time < g_nextTitle) return;
    g_nextTitle = g_time + 0.1;
    static const Color kFire[] = {Color(1, 0.1f, 0.05f, 1), Color(1, 0.45f, 0, 1), Color(1, 0.8f, 0.2f, 1), Color(1, 0.45f, 0, 1)};
    char buf[512];
    int n = 0;
    const char *word = "Blaze";
    for (int i = 0; word[i]; i++) {
        float t = (float)g_time * 1.5f - i * 0.35f;
        Color c;
        if (f_rainbow) c = Rainbow(t * 0.2f);
        else {
            float f = t - floorf(t / 4) * 4;          // 0..4 along the fire colors
            int k = (int)f;
            c = Lerp(kFire[k], kFire[(k + 1) % 4], f - k);
        }
        n += snprintf(buf + n, sizeof buf - n, "<b><color=#%02X%02X%02X>%c</color></b>", Hex(c.r), Hex(c.g), Hex(c.b), word[i]);
    }
    n += snprintf(buf + n, sizeof buf - n, " <color=grey>[</color><color=white>%d</color><color=grey>]</color>", g_page + 1);
    if (f_fps) n += snprintf(buf + n, sizeof buf - n, "<size=16>  %d fps</size>", (int)(g_fps + 0.5f));
    if (f_clock) {
        time_t now = time(nullptr);
        struct tm lt;
        localtime_r(&now, &lt);
        n += snprintf(buf + n, sizeof buf - n, "<size=16>  %d:%02d</size>", lt.tm_hour % 12 ? lt.tm_hour % 12 : 12, lt.tm_min);
    }
    SetText(g_title, buf);
}

static void ShowMenu(bool show) {
    if (show == g_menuShown) return;
    g_menuShown = show;
    if (show) { g_openTime = g_time; g_dirty = true; g_hover = -2; SetText(g_tipText, ""); }
    Show(g_menu, show);
    Show(g_pointer, show);
    g_touching = -2;
}

static float Dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Like ii's menu: the menu follows your hand and is tipped 45 degrees, so you read it
// by turning your palm toward your face. "Floating Menu" instead keeps the board above
// your hand, always facing you.
static void PlaceMenu() {
    Il2CppObject *menu = g_menu.get(), *board = g_board.get(), *head = g_head.get();
    Il2CppObject *hand = f_rightHand ? g_rightHand.get() : g_leftHand.get();
    if (!Alive(menu) || !Alive(board) || !hand || !head) return;
    Il2CppObject *t = Tr(menu), *bt = Tr(board);
    Vector3 handPos = G.trGetPosition[hand]();
    if (!G.trGetRotation.IsValid()) {                  // fallback: float above the hand, facing you
        Vector3 center = handPos + Vector3(0, 0.25f, 0), away = center - G.trGetPosition[head]();
        if (away.x * away.x + away.y * away.y + away.z * away.z < 1e-6f) return;
        G.trSetPosition[t](handPos);
        G.trSetPosition[bt](center);
        G.trSetRotation[bt](Quaternion::LookRotation(away));
        return;
    }
    const float s = 0.38268343f, c = 0.92387953f;                  // sin/cos of 22.5 degrees
    Quaternion rot = G.trGetRotation[hand]() * Quaternion(-s, 0, 0, c);   // tip -45 degrees on x
    if (f_rightHand) rot = rot * Quaternion(0, 0, 1, 0);           // mirrored: 180 degrees on z
    G.trSetPosition[t](handPos);
    G.trSetRotation[t](rot);
    G.trSetLocalPosition[bt](Vector3(0.1f, 0, 0));
    G.trSetLocalRotation[bt](Quaternion::LookRotation(Vector3(-1, 0, 0), Vector3(0, 0, 1)));
}

static void Press(Item *it) {
    switch (it->kind) {
    case OPEN:
        g_cat = it->target;
        g_page = 0;
        break;
    case TOGGLE:
        *it->flag = !*it->flag;
        Notify("[%s] %s", *it->flag ? "ON" : "OFF", it->label);
        if (it->fn) it->fn();                 // may replace the message above
        break;
    case ACTION:
        if (it->fn) it->fn();
        break;
    case STEPPER:
        break;                                // changed by Step(), from the << and >> buttons
    }
    g_dirty = true;
    ym::log("starter-menu: %s", it->label);
}

// << and >> of a setting row: change the number by one step, within its limits.
static void Step(Item *it, int dir) {
    int v = *it->val + dir * it->step;
    if (v < it->lo) v = it->lo;
    if (v > it->hi) v = it->hi;
    *it->val = v;
    if (it->fn) it->fn();
    Notify("%s: %d", it->label, v);
    g_dirty = true;
}

// Which part of the menu the pointer touches: row 0..7, -1 = <, kRows = >, -2 = nothing.
static int Touching() {
    Il2CppObject *board = g_board.get(), *pointer = g_pointer.get();
    if (!Alive(board) || !Alive(pointer)) return -2;
    Vector3 p = G.trInverseTransformPoint[Tr(board)](G.trGetPosition[Tr(pointer)]());
    if (p.z < -0.035f || p.z > 0.015f) return -2;
    if (fabsf(fabsf(p.x) - kSideX) < kSideW / 2 && fabsf(p.y) < kSideH / 2 && PageCount() > 1)
        return p.x < 0 ? -1 : kRows;
    if (fabsf(p.x) >= kBtnW / 2) return -2;
    for (int r = 0; r < kRows; r++)
        if (fabsf(p.y - RowY(r)) < kBtnH / 2) {
            g_hitZone = p.x < -(kBtnW / 2 - kStepW) ? 1 : p.x > (kBtnW / 2 - kStepW) ? 2 : 0;
            return r;
        }
    return -2;
}

static void Buzz() {
    if (!G.haptic.IsValid()) return;
    XRDevice d = G.deviceAtNode(f_rightHand ? 4 : 5);    // the hand that presses
    if (d.initialized) G.haptic(d.id, 0u, 0.35f, 0.04f);
}

static void CheckPress() {
    int hit = Touching();
    int key = (hit >= 0 && hit < kRows) ? hit * 4 + g_hitZone : hit;   // a row's << and >> count as separate buttons
    if (hit != g_hover) {                                 // tooltip for the button under your finger
        g_hover = hit;
        int i = g_page * kRows + hit;
        if (hit >= 0 && hit < kRows && i < (int)g_view.size()) SetText(g_tipText, TipFor(g_view[i]->label));
    }
    if (hit != -2 && key != g_touching && g_time >= g_nextPress) {   // one press per touch
        g_nextPress = g_time + 0.25;
        Buzz();
        if (hit >= 0 && hit < kRows) { g_flashRow = hit; g_flashUntil = g_time + 0.12; }
        if (hit == -1 || hit == kRows) {
            int n = PageCount();
            g_page = (g_page + (hit == kRows ? 1 : n - 1)) % n;
            g_dirty = true;
        } else {
            int i = g_page * kRows + hit;
            if (i < (int)g_view.size()) {
                Item *it = g_view[i];
                if (it->kind != STEPPER) Press(it);
                else if (g_hitZone != 0) Step(it, g_hitZone == 2 ? 1 : -1);
            }
        }
    }
    g_touching = key;
}

static void AnimateMenu() {
    float open = (float)(g_time - g_openTime) / 0.15f;     // pop open like ii's menu
    if (open <= 1.05f && Alive(g_menu.get())) {
        float k = open >= 1 ? 1 : 1 - (1 - open) * (1 - open);
        G.trSetLocalScale[Tr(g_menu.get())](Vector3(k, k, k));
    }
    if (g_flashRow != -2 && g_time >= g_flashUntil) { g_flashRow = -2; g_dirty = true; }
    float t = 0.5f + 0.5f * sinf((float)g_time * 2.0f);
    Color c = f_rainbow ? Rainbow((float)g_time * 0.15f) : Lerp(T().bgA, T().bgB, t);
    SetColor(g_bg.get(), c);
}

// ---- features (local only) -----------------------------------------------------

static Il2CppObject *PlayerTransform() {
    Il2CppObject *player = g_player.get();
    return player ? G.compTransform[player]() : nullptr;
}

// Your body's velocity, through the game's own player functions (Rigidbody as backup).
static bool GetVel(Vector3 &v) {
    Il2CppObject *player = g_player.get(), *rb = g_rigidbody.get();
    if (player && G.plGetVelocity.IsValid()) { v = G.plGetVelocity[player](); return true; }
    if (rb && G.rbGetVelocity.IsValid()) { v = G.rbGetVelocity[rb](); return true; }
    return false;
}
static void SetVel(Vector3 v) {
    Il2CppObject *player = g_player.get(), *rb = g_rigidbody.get();
    if (player && G.plSetVelocity.IsValid()) G.plSetVelocity[player](v);
    else if (rb && G.rbSetVelocity.IsValid()) G.rbSetVelocity[rb](v);
}

// Where your body is (the body collider), which is what teleports aim for.
static bool BodyPos(Vector3 &out) {
    Il2CppObject *player = g_player.get(), *t = PlayerTransform();
    if (!player || !t) return false;
    Il2CppObject *body = G.bodyCollider.IsValid() ? G.bodyCollider[player]() : nullptr;
    out = body ? G.trGetPosition[G.compTransform[body]()]() : G.trGetPosition[t]();
    return true;
}
static void StopMoving() { SetVel(Vector3(0, 0, 0)); }
// Moves your body to pos, using the game's own TeleportTo when it has one.
static void TeleportPlayer(Vector3 pos) {
    Il2CppObject *player = g_player.get(), *t = PlayerTransform();
    Vector3 body;
    if (!player || !t || !BodyPos(body)) return;
    Vector3 target = pos - body + G.trGetPosition[t]();
    if (G.teleportTo.IsValid()) G.teleportTo[player](target);
    else G.trSetPosition[t](target);
    StopMoving();
}

static void SaveSpot() {
    if (!BodyPos(g_spot)) return;
    g_haveSpot = true;
    if (!Alive(g_spotCube.get())) {                      // a cube marks where you were
        Il2CppObject *c = Cube(T().bgB, false);
        if (c) { G.trSetLocalScale[Tr(c)](Vector3(0.2f, 0.2f, 0.2f)); g_spotCube.set(c); }
    }
    if (Alive(g_spotCube.get())) G.trSetPosition[Tr(g_spotCube.get())](g_spot);
    Notify("Position saved");
}
static void GoToSpot() {
    if (!g_haveSpot) { Notify("Save a position first"); return; }
    TeleportPlayer(g_spot);
    Notify("Teleported to your saved position");
}

static void NextTheme() { g_theme = (g_theme + 1) % kThemeCount; Notify("Theme: %s", T().name); }
static void AllOff() {
    for (auto &it : g_items) if (it.kind == TOGGLE && it.flag != &f_notify) *it.flag = false;   // ticks undo each mod
    g_dance = -1;
    g_dupeArmed = 0;
    g_itemQueue.clear();
    PointerToHand();
    Notify("Everything off");
}

static void PlatformFor(bool held, Ref &platform, Ref &hand) {
    if (!held || !f_platforms) {
        Kill(platform);
        return;
    }
    if (platform.get()) return;                             // already standing on one
    if (!hand.get()) return;
    Il2CppObject *p = Cube(T().button, true);               // keeps its collider: you stand on it
    if (!p) return;
    // Hands only collide with the layers in the player's locomotionEnabledLayers (like
    // Gorilla Tag), so the platform goes on the first of those layers.
    static int layer = -2;
    if (layer == -2) {
        layer = -1;
        int mask = G.locomotionLayers.IsValid() && g_player.get() ? G.locomotionLayers[g_player.get()]() : 0;
        for (int i = 0; i < 32 && layer < 0; i++) if (mask & (1 << i)) layer = i;
        ym::log("starter-menu: hand layers 0x%x, platforms on layer %d", mask, layer);
    }
    if (layer >= 0 && G.goSetLayer.IsValid()) G.goSetLayer[p](layer);
    Il2CppObject *t = Tr(p);
    G.trSetPosition[t](G.trGetPosition[hand.get()]() - Vector3(0, 0.08f, 0));
    G.trSetLocalScale[t](Vector3(0.28f, 0.02f, 0.28f));
    platform.set(p);
}

// Teleport Gun: hold right grip to aim (a line and a ball show where you land), press
// the right trigger to go there.
static Ref g_gunLine, g_gunLineGo, g_gunBall;
static bool g_gunTrigger = false;
static void TeleportGunTick(const Buttons &in) {
    bool aiming = (f_tpGun || f_itemGun) && in.rgrip;
    if (!aiming) {
        Show(g_gunLineGo, false);
        Show(g_gunBall, false);
        g_gunTrigger = in.rtrig;
        return;
    }
    Il2CppObject *hand = g_rightHand.get();
    if (!hand || !G.raycast.IsValid() || !G.lrSetPosition.IsValid()) return;
    if (!g_gunBall.get()) {                              // made once per room
        Il2CppObject *ball = Prim(0, T().bgB, false);
        Il2CppObject *lineGo = Class("UnityEngine", "GameObject").CreateNewObjectParameters(CreateMonoString("BlazeGunLine"));
        Il2CppObject *line = lineGo && G.tLine ? G.goAddComponent[lineGo](G.tLine) : nullptr;
        if (!ball || !line) return;
        G.trSetLocalScale[Tr(ball)](Vector3(0.1f, 0.1f, 0.1f));
        if (Il2CppObject *m = G.rendererGetMaterial[line]()) {
            if (G.shader.get()) G.matSetShader[m](G.shader.get());
            G.matSetColor[m](T().bgB);
        }
        G.lrSetStartWidth[line](0.025f);
        G.lrSetEndWidth[line](0.025f);
        G.lrSetPositionCount[line](2);
        G.lrSetUseWorldSpace[line](true);
        g_gunBall.set(ball);
        g_gunLine.set(line);
        g_gunLineGo.set(lineGo);
    }
    Vector3 start = G.trGetPosition[hand](), dir = G.trGetForward[hand]();
    BNM::Structures::Unity::RaycastHit hit{};
    bool found = G.raycast(start + dir * 0.25f, dir, &hit, 512.0f, -5);   // Physics.DefaultRaycastLayers
    Vector3 end = found ? hit.point : start + dir * 512.0f;
    Show(g_gunLineGo, true);
    Show(g_gunBall, true);
    G.trSetPosition[Tr(g_gunBall.get())](end);
    G.lrSetPosition[g_gunLine.get()](0, start);
    G.lrSetPosition[g_gunLine.get()](1, end);
    SetColor(g_gunBall.get(), in.rtrig ? T().buttonOn : T().bgB);
    if (in.rtrig && !g_gunTrigger && found) {
        if (f_itemGun) {                                  // Spawn Item Gun: an item at that spot
            if (g_time >= g_nextItem && LeftHandItem()) {
                g_nextItem = g_time + 0.5;
                g_itemQueue.push_back({end + Vector3(0, 0.3f, 0), Vector3(0, 0, 0)});
            }
        } else TeleportPlayer(end + Vector3(0, 1.0f, 0));
    }
    g_gunTrigger = in.rtrig;
}

// ---- vector helpers ----------------------------------------------------------------

static Vector3 Cross(Vector3 a, Vector3 b) { return Vector3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static Vector3 Flat(Vector3 v) {                      // v without its up/down part, length 1
    v.y = 0;
    float l = sqrtf(v.x * v.x + v.z * v.z);
    return l > 1e-4f ? v * (1.0f / l) : Vector3(0, 0, 1);
}

// ---- rig mods: dances -----------------------------------------------------------
// While you hold the left trigger, your hands are moved through a dance instead of
// following your controllers. The game sends your hand positions to the room as
// usual, so other players see you dance. Nothing extra is sent.

// The dance buttons work like radio buttons: picking one turns the others off,
// pressing the picked one again turns dancing off.
static void DancePicked() {
    int picked = -1;
    for (int i = 0; i < kDances; i++) if (g_danceSel[i] && i != g_dance) picked = i;
    for (int i = 0; i < kDances; i++) g_danceSel[i] = (i == picked);
    g_dance = picked;
    if (picked >= 0) Notify("Hold left trigger to dance");
}
static float Rand01() { static uint32_t x = 2463534242u; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return (x & 0xffff) / 65535.0f; }

// The controller trackers would put your hands back every frame, so they are paused
// while you dance (on the hand and the two objects above it) and turned back on after.
static Ref g_paused[12];
static int g_pausedN = 0;
static bool g_dancing = false;
static void PauseTrackers(bool pause) {
    if (!pause) {
        for (int i = 0; i < g_pausedN; i++) {
            if (Alive(g_paused[i].get())) G.behaviourSetEnabled[g_paused[i].get()](true);
            g_paused[i].clear();
        }
        g_pausedN = 0;
        return;
    }
    if (!G.behaviourSetEnabled.IsValid() || !G.trGetParent.IsValid()) return;
    for (Il2CppObject *t : {g_leftHand.get(), g_rightHand.get()})
        for (int up = 0; up < 3 && t; up++, t = G.trGetParent[t]())
            for (BNM::MonoType *type : G.trackerTypes) {
                Il2CppObject *go = G.compGameObject[t]();
                Il2CppObject *c = go ? G.goGetComponent[go]((Il2CppObject *)type) : nullptr;
                if (!c || !G.behaviourGetEnabled[c]() || g_pausedN >= 12) continue;
                G.behaviourSetEnabled[c](false);
                g_paused[g_pausedN++].set(c);
            }
}

static void DanceTick(const Buttons &in, float dt) {
    Il2CppObject *head = g_head.get(), *lh = g_leftHand.get(), *rh = g_rightHand.get();
    bool want = g_dance >= 0 && in.ltrig && head && lh && rh;
    if (want != g_dancing) {
        PauseTrackers(want);
        g_dancing = want;
        if (want) ym::log("starter-menu: dancing, paused %d trackers", g_pausedN);
    }
    if (!want) return;
    Vector3 hp = G.trGetPosition[head](), f = Flat(G.trGetForward[head]()), up(0, 1, 0);
    Vector3 r = Cross(up, f);                            // your right
    Vector3 shoulder = hp + up * -0.25f;
    float t = (float)g_time, s = sinf(t * 6), c = cosf(t * 6);
    Vector3 L, R;                                        // hand positions
    switch (g_dance) {
    case 0:   // Wave: right hand up and waving, left hand on the hip
        R = shoulder + r * (0.35f + 0.12f * s) + up * 0.45f + f * 0.1f;
        L = shoulder + r * -0.25f + up * -0.25f + f * 0.05f;
        break;
    case 1:   // Helicopter: both hands spin around you
        R = shoulder + r * (0.6f * cosf(t * 8)) + f * (0.6f * sinf(t * 8)) + up * 0.15f;
        L = shoulder - r * (0.6f * cosf(t * 8)) - f * (0.6f * sinf(t * 8)) + up * 0.15f;
        break;
    case 2:   // T-Pose
        R = shoulder + r * 0.7f + up * 0.1f;
        L = shoulder - r * 0.7f + up * 0.1f;
        break;
    case 3:   // Windmill: big circles at your sides
        R = shoulder + r * 0.4f + up * (0.5f * sinf(-t * 5)) + f * (0.5f * cosf(-t * 5));
        L = shoulder - r * 0.4f - up * (0.5f * sinf(-t * 5)) - f * (0.5f * cosf(-t * 5));
        break;
    case 4: {  // Swim: breaststroke
        float p = t * 3;
        R = shoulder + f * (0.3f + 0.25f * cosf(p)) + r * (0.08f + 0.35f * fabsf(sinf(p)));
        L = shoulder + f * (0.3f + 0.25f * cosf(p)) - r * (0.08f + 0.35f * fabsf(sinf(p)));
        break;
    }
    case 5: {  // Jumping jacks: arms from down by your sides to up over your head
        float a = (sinf(t * 6) * 0.5f + 0.5f) * 3.1f - 1.55f;
        R = shoulder + r * (0.6f * cosf(a)) + up * (0.6f * sinf(a));
        L = shoulder - r * (0.6f * cosf(a)) + up * (0.6f * sinf(a));
        break;
    }
    case 6:   // Spaz: hands in front, shaking everywhere
        R = shoulder + f * 0.35f + r * 0.2f + Vector3(Rand01() - 0.5f, Rand01() - 0.5f, Rand01() - 0.5f) * 0.3f;
        L = shoulder + f * 0.35f - r * 0.2f + Vector3(Rand01() - 0.5f, Rand01() - 0.5f, Rand01() - 0.5f) * 0.3f;
        break;
    default:  // Beyblade: hands spin fast around your waist
        R = shoulder - up * 0.3f + r * (0.5f * cosf(t * 20)) + f * (0.5f * sinf(t * 20));
        L = shoulder - up * 0.3f - r * (0.5f * cosf(t * 20)) - f * (0.5f * sinf(t * 20));
        break;
    }
    // Keep each hand within arm's reach of its shoulder, then glide there instead of
    // jumping, so the arms move smoothly for you and for everyone watching.
    auto reach = [&](Vector3 hand, Vector3 sh) {
        Vector3 d = hand - sh;
        float len = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
        return len > 0.7f ? sh + d * (0.7f / len) : hand;
    };
    R = reach(R, shoulder + r * 0.18f);
    L = reach(L, shoulder - r * 0.18f);
    float k = g_dance == 6 ? 1.0f : fminf(1.0f, dt * 14.0f);     // Spaz stays twitchy
    G.trSetPosition[lh](G.trGetPosition[lh]() + (L - G.trGetPosition[lh]()) * k);
    G.trSetPosition[rh](G.trGetPosition[rh]() + (R - G.trGetPosition[rh]()) * k);
    Quaternion look = Quaternion::LookRotation(f, up);
    G.trSetRotation[lh](look);
    G.trSetRotation[rh](look);
}

// ---- items (everyone sees these) ------------------------------------------------
// Items are made with the game's own item message (RpcCreateItem on the room's
// NetworkMessenger), the same one the game sends when an item appears. Owner-approved
// 2026-10-31. Every way of spawning has a cooldown, and big batches are sent a few per
// frame, so the room can't be flooded.


static void Put(Il2CppObject *arr, size_t i, void *v) { ((void **)((uint8_t *)arr + 0x20))[i] = v; }

// Builds RpcCreateItem's arguments from its own parameter list, in the order the game
// uses: item id, item key, spawn kind 1, no owner, position, rotation, velocity, spin,
// properties, 1, 1.
static bool SendItem(const char *key, Vector3 pos, Quaternion rot, Vector3 vel, char *idOut = nullptr) {
    auto *mi = G.rpcCreateItem;
    Il2CppObject *nm = G.netLocal.IsValid() ? G.netLocal() : nullptr;
    if (!mi || !nm || !G.pvRpc.IsValid() || !G.arrayNew || !G.valueBox || !G.tPhotonView) return false;
    Il2CppObject *go = G.compGameObject[nm]();
    Il2CppObject *pv = go ? G.goGetComponent[go]((Il2CppObject *)G.tPhotonView) : nullptr;
    if (!pv) return false;
    // The last parameter (PhotonMessageInfo) is filled in by Photon on the receiving
    // side; it is never sent.
    int count = mi->parameters_count;
    if (count > 0 && TypeName(mi->parameters[count - 1]) == "Photon.Pun.PhotonMessageInfo") count--;
    auto *args = (Il2CppObject *)G.arrayNew(G.objectClass, count);
    if (!args) return false;
    char id[8];
    static const char kChars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < 4; i++) id[i] = kChars[(int)(Rand01() * 35.99f)];
    id[4] = 0;
    if (idOut) memcpy(idOut, id, 5);
    int strings = 0, vectors = 0, others = 0;
    for (int i = 0; i < count; i++) {
        std::string tn = TypeName(mi->parameters[i]);
        Class pc(mi->parameters[i]);
        auto *k = pc.IsValid() ? pc.GetClass() : nullptr;
        void *v = nullptr;
        if (tn == "System.String") {
            if (strings == 0) v = CreateMonoString(id);
            else if (strings == 1) v = CreateMonoString(key);
            strings++;                                   // later strings stay null
        } else if (tn == "UnityEngine.Vector3") {
            Vector3 vec = vectors == 0 ? pos : vectors == 1 ? vel : Vector3(0, 0, 0);
            vectors++;
            v = G.valueBox(k, &vec);
        } else if (tn == "UnityEngine.Quaternion") {
            v = G.valueBox(k, &rot);
        } else if (tn == "System.String[]") {
            v = G.arrayNew(G.stringClass, 0);
        } else if (k && k->byval_arg.valuetype) {
            uint8_t raw[64] = {};
            raw[0] = 1;                                 // spawn kind 1 / the two bytes = 1
            // Photon can't send the game's enum types, only plain numbers (like the kit's
            // Int32 1), so an enum goes as its underlying number type.
            auto *boxAs = k->enumtype && k->element_class ? k->element_class : k;
            v = G.valueBox(boxAs, raw);
            others++;
        }
        Put(args, i, v);
    }
    static bool loggedTypes = false;
    if (!loggedTypes) {                                   // once: what the game expects, for fixing
        loggedTypes = true;
        std::string sig;
        for (int i = 0; i < mi->parameters_count; i++) {
            Class pc(mi->parameters[i]);
            auto *k = pc.IsValid() ? pc.GetClass() : nullptr;
            std::string tn = TypeName(mi->parameters[i]);
            if (tn.size() > 30) tn = k && k->byval_arg.valuetype ? (k->enumtype ? "enum" : "struct" + std::to_string(k->instance_size - 16)) : "class";
            sig += (i ? "," : "") + tn;
        }
        ym::log("starter-menu: item message takes %s", sig.c_str());
    }
    try {
        G.pvRpc[pv](CreateMonoString("RpcCreateItem"), 0, args);      // 0 = everyone in the room
    } catch (BNM::IL2CPP::Il2CppExceptionWrapper &w) {   // the game said no: log why, don't crash the menu
        auto *ex = w.ex;
        auto *msg = ex ? (BNM::Structures::Mono::String *)ex->message : nullptr;
        auto *cls = ex ? (BNM::Structures::Mono::String *)ex->className : nullptr;
        ym::log("starter-menu: item message error %s: %s", cls ? cls->str().c_str() : "?", msg ? msg->str().c_str() : "?");
        return false;
    }
    return true;
}

static void QueueItems(const std::vector<PendingItem> &list) {
    if (!g_itemKey[0]) return;
    if (g_time < g_nextBatch) { Notify("Wait a moment"); return; }
    g_nextBatch = g_time + 3.0;
    for (auto &p : list) if (g_itemQueue.size() < 32) g_itemQueue.push_back(p);
}

static void ItemQueueTick() {
    for (int sent = 0; sent < 2 && !g_itemQueue.empty(); sent++) {   // 2 a frame at most
        PendingItem p = g_itemQueue.front();
        g_itemQueue.erase(g_itemQueue.begin());
        char id[8] = {};
        bool ok = SendItem(g_itemKey, p.pos, Quaternion(0, 0, 0, 1), p.vel, id);
        if (ok && p.track && g_spawned.size() < 48) g_spawned.push_back(id);
        static int logged = 0;
        if (logged++ < 3) ym::log("starter-menu: spawn %s ok=%d", g_itemKey, ok ? 1 : 0);
        if (!ok) { g_itemQueue.clear(); Notify("Item spawning isn't ready in this room"); }
    }
    for (int sent = 0; sent < 2 && !g_despawnQueue.empty(); sent++) {     // Despawn Items, 2 a frame
        std::string id = g_despawnQueue.back();
        g_despawnQueue.pop_back();
        if (!SendDespawn(id.c_str())) { g_despawnQueue.clear(); Notify("Despawn isn't ready in this room"); }
    }
}

// The grid in front of you: 3 across, 3 deep, and 3 high for 3x3x3.
static void SpawnGrid(int count) {
    Il2CppObject *head = g_head.get();
    if (!head) return;
    Vector3 f = Flat(G.trGetForward[head]()), r = Cross(Vector3(0, 1, 0), f);
    Vector3 base = G.trGetPosition[head]() + f * 1.2f + Vector3(0, -0.4f, 0);
    std::vector<PendingItem> list;
    for (int y = 0; y < (count > 9 ? 3 : 1); y++)
        for (int z = 0; z < 3; z++)
            for (int x = 0; x < 3; x++)
                list.push_back({base + r * ((x - 1) * 0.45f) + f * (z * 0.45f) + Vector3(0, y * 0.45f, 0), Vector3(0, 0, 0)});
    QueueItems(list);
}
// The item you're holding in your LEFT hand right now (owner 2026-10-31: items spawn as
// whatever is in your left hand when you press). One scene search per press.
static bool LeftHandItem() {
    Il2CppObject *hand = g_leftHand.get();
    if (!hand || !G.tItem || !G.itemKey.IsValid() || !(G.findScene.IsValid() || G.findSceneByType.IsValid())) return false;
    auto *arr = (uint8_t *)(G.findScene.IsValid() ? G.findScene((Il2CppObject *)G.tItem)
                                                  : G.findSceneByType((Il2CppObject *)G.tItem, 0));
    size_t n = arr ? *(uintptr_t *)(arr + 0x18) : 0;
    Vector3 hp = G.trGetPosition[hand]();
    Il2CppObject *best = nullptr;
    float bestD = 0.4f * 0.4f;
    for (size_t i = 0; i < n; i++) {
        Il2CppObject *it = ((Il2CppObject **)(arr + 0x20))[i];
        if (!Alive(it) || (G.itemHeld.IsValid() && !G.itemHeld[it]())) continue;
        Vector3 d = G.trGetPosition[G.compTransform[it]()]() - hp;
        float dd = d.x * d.x + d.y * d.y + d.z * d.z;
        if (dd < bestD) { bestD = dd; best = it; }
    }
    auto *key = best ? G.itemKey[best]() : nullptr;
    if (!key) { Notify("Hold an item in your left hand"); return false; }
    snprintf(g_itemKey, sizeof g_itemKey, "%s", key->str().c_str());
    return true;
}
static void ArmDupe9() { if (LeftHandItem()) SpawnGrid(9); }
static void ArmDupe27() { if (LeftHandItem()) SpawnGrid(27); }

// Spawn On Hand: each right-trigger press (grip open) spawns the item at your hand.
static bool g_onHandTrigger = false;
static void SpawnOnHandTick(const Buttons &in) {
    bool press = in.rtrig && !g_onHandTrigger;
    g_onHandTrigger = in.rtrig;
    Il2CppObject *hand = g_rightHand.get();
    if (!f_spawnOnHand || !press || in.rgrip || g_menuShown || !hand || g_time < g_nextItem) return;
    if (!LeftHandItem()) return;
    g_nextItem = g_time + 0.3;
    g_itemQueue.push_back({G.trGetPosition[hand]() + G.trGetForward[hand]() * 0.1f, Vector3(0, 0, 0)});
}

// ---- movement and fun mods ---------------------------------------------------------
// These change only your own game. The exceptions are Spawn Item and Despawn Items: the
// room sees those items, like the ones on the Items page.

static const float kMaxVelFly = 200.0f;                  // Velocity Fly stops speeding up here
static const float kTReach = 0.75f, kTRadius = 0.22f;    // Airplane Fly circles: how far out from your chest, how big
static const float kArmScale = 4.0f;                     // Long Arms: how many times longer
static float g_velFly = 0;                               // Velocity Fly's built-up speed
static bool g_flyHeld = false;                           // Fly / Airplane Fly moved you last frame
static bool g_airShown = false;
static int g_airState = -1;

static float Len(Vector3 v) { return sqrtf(Dot(v, v)); }
static void KeepAfloat() {                               // the game's own "no gravity for a moment"
    if (G.plNoGravityFor.IsValid() && g_player.get()) G.plNoGravityFor[g_player.get()](0.1f);
}

// ---- log helpers: the game allows 20 log lines a second, so lines go out one at a time ----
static std::vector<std::string> g_logQueue;
static double g_nextLog = 0;
static void QueueLog(const char *fmt, ...) {
    if (g_logQueue.size() >= 120) return;
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    g_logQueue.push_back(buf);
}
static void FlushLog() {
    if (g_logQueue.empty() || g_time < g_nextLog) return;
    ym::log("starter-menu: %s", g_logQueue.front().c_str());
    g_logQueue.erase(g_logQueue.begin());
    g_nextLog = g_time + 0.07;
}

// For a feature that needs a part of the game this menu doesn't know yet: write the names
// of game classes (and MasterPlayer fields/methods) that look related to the log, so the
// right ones can be wired in. Nothing in the game is changed.
static void ProbeGame(const char *feature, std::initializer_list<const char *> words) {
    QueueLog("probe '%s': game parts that look related", feature);
    Image img("Assembly-CSharp.dll");
    int shown = 0;
    if (img.IsValid())
        for (auto &c : img.GetClasses(false)) {
            auto *k = c.GetClass();
            if (!k || !k->name || !HasWord(Lower(k->name), words)) continue;
            QueueLog("class %s", k->name);
            if (++shown >= 14) break;
        }
    Class player("", "MasterPlayer", img);
    if (!player.IsValid()) return;
    shown = 0;
    for (auto &f : player.GetFields(true)) {
        auto *fi = f.GetInfo();
        if (!fi || !fi->name || !HasWord(Lower(fi->name), words)) continue;
        QueueLog("MasterPlayer field %s", fi->name);
        if (++shown >= 10) break;
    }
    shown = 0;
    for (auto &m : player.GetMethods(true)) {
        auto *mi = m.GetInfo();
        if (!mi || !mi->name || !HasWord(Lower(mi->name), words)) continue;
        QueueLog("MasterPlayer method %s (%d args)", mi->name, (int)mi->parameters_count);
        if (++shown >= 10) break;
    }
}
static void ProbeRpcs() {
    Class net("", "NetworkMessenger", Image("Assembly-CSharp.dll"));
    if (!net.IsValid()) return;
    QueueLog("probe: NetworkMessenger messages");
    int shown = 0;
    for (auto &m : net.GetMethods(false)) {
        auto *mi = m.GetInfo();
        if (!mi || !mi->name || strncmp(mi->name, "Rpc", 3) != 0) continue;
        QueueLog("%s (%d args)", mi->name, (int)mi->parameters_count);
        if (++shown >= 30) break;
    }
}
static void NotWired(const char *what, bool &flag, std::initializer_list<const char *> words) {
    flag = false;                                           // nothing to switch on yet
    ProbeGame(what, words);
    Notify("%s isn't wired up yet (see the log)", what);
}

// ---- Fly, Velocity Fly, Airplane Fly (right trigger, right grip open) -----------------
// Fly: hold the trigger, fly where you look. Velocity Fly: hold the trigger, shoot where your
// right hand points, +0.01 speed every millisecond; the speed resets when you let go.
// Airplane Fly: hold both hands in the two circles beside you (a T-pose) to fly where you look.
static void AirMarker(Ref &r) {
    if (Alive(r.get())) return;
    Il2CppObject *p = Prim(0, T().bgB, false);
    if (!p) return;
    G.trSetLocalScale[Tr(p)](Vector3(0.1f, 0.1f, 0.1f));
    r.set(p);
}
static void FlightTick(const Buttons &in, float dt) {
    Il2CppObject *head = g_head.get(), *lh = g_leftHand.get(), *rh = g_rightHand.get();
    if (!f_airplane && g_airShown) { Show(g_airL, false); Show(g_airR, false); g_airShown = false; }
    if (!f_fly && !f_velocityFly && !f_airplane) {
        if (g_flyHeld) { StopMoving(); g_flyHeld = false; }
        g_velFly = 0;
        return;
    }
    if (!head || !lh || !rh || !G.trGetForward.IsValid()) return;
    bool free = !g_menuShown && !g_dancing;
    bool trig = in.rtrig && !in.rgrip && free;
    bool plane = false;
    if (f_airplane && free && G.trGetRight.IsValid()) {
        Vector3 r = Flat(G.trGetRight[head]()), chest = G.trGetPosition[head]() + Vector3(0, -0.25f, 0);
        Vector3 cl = chest - r * kTReach, cr = chest + r * kTReach;
        bool inL = Len(G.trGetPosition[lh]() - cl) < kTRadius, inR = Len(G.trGetPosition[rh]() - cr) < kTRadius;
        plane = inL && inR;
        AirMarker(g_airL);
        AirMarker(g_airR);
        if (!g_airShown) { Show(g_airL, true); Show(g_airR, true); g_airShown = true; g_airState = -1; }
        if (Alive(g_airL.get())) G.trSetPosition[Tr(g_airL.get())](cl);
        if (Alive(g_airR.get())) G.trSetPosition[Tr(g_airR.get())](cr);
        int st = (inL ? 1 : 0) | (inR ? 2 : 0);
        if (st != g_airState) {                            // green while a hand is inside its circle
            g_airState = st;
            SetColor(g_airL.get(), inL ? Color(0.3f, 1.0f, 0.3f, 1) : T().bgB);
            SetColor(g_airR.get(), inR ? Color(0.3f, 1.0f, 0.3f, 1) : T().bgB);
        }
    } else if (g_airShown) { Show(g_airL, false); Show(g_airR, false); g_airShown = false; }

    bool vel = f_velocityFly && trig;
    bool fly = !vel && f_fly && trig;
    bool air = !vel && !fly && plane;
    if (vel) {
        g_velFly = fminf(g_velFly + 10.0f * dt, kMaxVelFly);      // +0.01 a millisecond = +10 a second
        SetVel(G.trGetForward[rh]() * g_velFly);
        KeepAfloat();
    } else g_velFly = 0;
    if (fly || air) {
        SetVel(G.trGetForward[head]() * (float)(fly ? g_flySpeed : g_airSpeed));
        KeepAfloat();
    }
    bool held = fly || air;
    if (!held && g_flyHeld) StopMoving();
    g_flyHeld = held;
}

// ---- Speed Boost (passive) --------------------------------------------------------
// Scales the game's own walking/jump push values (maxJumpSpeed and jumpMultiplier) by
// 1 + Boost Speed / 10, so 10 is double speed and a negative number slows you down. The
// original values are put back when it goes off.
static float g_oldJumpSpeed = 0, g_oldJumpMultiplier = 0;
static Il2CppObject *g_boosted = nullptr;      // which player has the boost (compared only)
static int g_boostApplied = 0;
static float BoostFactor() { float m = 1.0f + g_boostSpeed * 0.1f; return m < 0.1f ? 0.1f : m; }
static void ApplySpeedBoost() {
    Il2CppObject *player = g_player.get();
    if (!player || !G.maxJumpSpeed.IsValid() || !G.jumpMultiplier.IsValid()) return;
    if (f_speedBoost) {
        if (g_boosted != player) {
            g_oldJumpSpeed = G.maxJumpSpeed[player]();
            g_oldJumpMultiplier = G.jumpMultiplier[player]();
            g_boosted = player;
            g_boostApplied = 12345;                  // force a write below
        }
        if (g_boostApplied != g_boostSpeed) {
            float m = BoostFactor();
            G.maxJumpSpeed[player] = g_oldJumpSpeed * m;
            G.jumpMultiplier[player] = g_oldJumpMultiplier * m;
            g_boostApplied = g_boostSpeed;
        }
    } else if (g_boosted) {
        if (player == g_boosted) {
            G.maxJumpSpeed[player] = g_oldJumpSpeed;
            G.jumpMultiplier[player] = g_oldJumpMultiplier;
        }
        g_boosted = nullptr;
    }
}
static void SpeedBoostToggled() {
    static bool probed = false;                      // once: list the game's potion/effect parts in the log
    if (f_speedBoost && !probed) { probed = true; ProbeGame("speed potion", {"potion", "speedboost", "effect"}); }
}

// ---- Long Arms ---------------------------------------------------------------------
// Moves your own hands, like the dances do, so the room sees the long arms the way it sees
// any pose: the game sends your hands as usual and nothing extra goes out. The hands go back
// to normal while the menu is open.
// To stretch an arm the controller trackers have to be paused (they would put the hand back),
// so the real controller position is read from the headset's input instead. That position is
// in the space of one of the hand's parents; the first frames check which one by comparing
// with where the hand really is. If nothing matches, Long Arms switches itself off.
static int g_laState = 0;                      // 0 starting, 1 checking, 2 working, 3 not supported
static int g_laChecks = 0, g_laSpaceL = -1, g_laSpaceR = -1;
static bool g_laOwns = false, g_laInit = false;
static double g_laNextPause = 0;
static Method<bool> g_tryVec3, g_tryQuat;
static Ref g_usagePos, g_usageRot;
static void LongArmsToggled() { g_laState = 0; g_laChecks = 0; g_laSpaceL = g_laSpaceR = -1; }

static bool LaInit() {
    if (g_laInit) return g_tryVec3.IsValid() && g_tryQuat.IsValid();
    g_laInit = true;
    Class dev("UnityEngine.XR", "InputDevices");
    if (!dev.IsValid()) return false;
    g_tryVec3 = dev.GetMethod("TryGetFeatureValue_Vector3", 3);
    g_tryQuat = dev.GetMethod("TryGetFeatureValue_Quaternion", 3);
    g_usagePos.set((Il2CppObject *)CreateMonoString("DevicePosition"));
    g_usageRot.set((Il2CppObject *)CreateMonoString("DeviceRotation"));
    return g_tryVec3.IsValid() && g_tryQuat.IsValid();
}
// The controller's real position and rotation in the world, as one of the hand's parents sees it.
static bool ReadController(int node, Il2CppObject *hand, int &space, Vector3 &pos, Quaternion &rot) {
    XRDevice d = G.deviceAtNode(node);
    if (!d.initialized) return false;
    Vector3 lp;
    Quaternion lr;
    if (!g_tryVec3(d.id, (BNM::Structures::Mono::String *)g_usagePos.get(), &lp)) return false;
    if (!g_tryQuat(d.id, (BNM::Structures::Mono::String *)g_usageRot.get(), &lr)) return false;
    Il2CppObject *a = G.trGetParent[hand]();
    Vector3 handPos = G.trGetPosition[hand]();
    int best = -1;
    float bestErr = 0.10f;
    for (int k = 0; k < 3 && a; k++, a = G.trGetParent[a]()) {
        if (space >= 0 && k != space) continue;
        Vector3 w = G.trTransformPoint[a](lp), e = w - handPos;
        float err = sqrtf(Dot(e, e));
        if (space >= 0 || err < bestErr) { best = k; bestErr = err; pos = w; rot = G.trGetRotation[a]() * lr; }
        if (space >= 0) break;
    }
    if (best < 0) return false;
    if (space < 0) space = best;
    return true;
}
static void LongArmsFail(const char *why) {
    g_laState = 3;
    if (g_laOwns && !g_dancing) PauseTrackers(false);
    g_laOwns = false;
    f_longArms = false;
    g_dirty = true;
    ym::log("starter-menu: long arms off: %s", why);
    Notify("Long Arms isn't supported here");
}
static void LongArmsTick() {
    Il2CppObject *head = g_head.get(), *lh = g_leftHand.get(), *rh = g_rightHand.get();
    bool want = f_longArms && !g_menuShown && !g_dancing && head && lh && rh;
    if (!want) {
        if (g_laOwns && !g_dancing) { PauseTrackers(false); g_laOwns = false; }   // the menu and the dances need normal hands
        if (!f_longArms) g_laState = 0;
        return;
    }
    if (!LaInit() || !G.trTransformPoint.IsValid() || !G.trGetParent.IsValid() || !G.trGetRotation.IsValid()) { LongArmsFail("input functions missing"); return; }
    Vector3 pl, pr;
    Quaternion ql, qr;
    if (g_laState < 2) {                                           // trackers still on: find out how to read the controllers
        bool okL = ReadController(4, lh, g_laSpaceL, pl, ql), okR = ReadController(5, rh, g_laSpaceR, pr, qr);
        if (okL && okR) { g_laState = 2; ym::log("starter-menu: long arms ready (spaces %d/%d)", g_laSpaceL, g_laSpaceR); }
        else if (++g_laChecks > 60) LongArmsFail("controller positions don't match the hands");
        return;
    }
    if (g_pausedN == 0 && g_time >= g_laNextPause) {               // pause the trackers (retry twice a second)
        g_laNextPause = g_time + 0.5;
        PauseTrackers(true);
        g_laOwns = g_pausedN > 0;
        if (!g_laOwns) ym::log("starter-menu: long arms: couldn't pause the hand trackers");
    }
    if (!ReadController(4, lh, g_laSpaceL, pl, ql) || !ReadController(5, rh, g_laSpaceR, pr, qr)) return;
    Vector3 chest = G.trGetPosition[head]() + Vector3(0, -0.25f, 0);
    G.trSetPosition[lh](chest + (pl - chest) * kArmScale);
    G.trSetRotation[lh](ql);
    G.trSetPosition[rh](chest + (pr - chest) * kArmScale);
    G.trSetRotation[rh](qr);
}

// ---- Boundary Pass / Outer Boundary Pass ----------------------------------------------
// Looks once (when switched on, and once after a room loads) for colliders whose object
// name says boundary / barrier / invisible wall (and "outer" for the outer one) and
// switches them off; switching the mod off puts them back. If nothing matches, the log gets
// the first few collider names so the right words can be added.
struct PassSet { Ref cols[48]; int n = 0; bool scanned = false; double nextScan = 0; };
static PassSet g_inner, g_outer;
static void PassRelease(PassSet &s) {
    for (int i = 0; i < s.n; i++) {
        if (Alive(s.cols[i].get())) G.colliderSetEnabled[s.cols[i].get()](true);
        s.cols[i].clear();
    }
    s.n = 0;
    s.scanned = false;
}
static void PassScan(PassSet &s, bool outer, const char *what) {
    s.scanned = true;
    if (!G.objGetName.IsValid() || !G.colliderSetEnabled.IsValid() || !G.behaviourGetEnabledCollider.IsValid() || !G.tCollider ||
        !(G.findScene.IsValid() || G.findSceneByType.IsValid())) { Notify("%s isn't supported here", what); return; }
    auto *arr = (uint8_t *)(G.findScene.IsValid() ? G.findScene((Il2CppObject *)G.tCollider)
                                                  : G.findSceneByType((Il2CppObject *)G.tCollider, 0));
    size_t n = arr ? *(uintptr_t *)(arr + 0x18) : 0;
    std::vector<std::string> seen;
    for (size_t i = 0; i < n && s.n < 48; i++) {
        Il2CppObject *c = ((Il2CppObject **)(arr + 0x20))[i];
        if (!Alive(c)) continue;
        Il2CppObject *go = G.compGameObject[c]();
        auto *nm = go ? G.objGetName[go]() : nullptr;
        if (!nm) continue;
        std::string name = Lower(nm->str().c_str());
        bool isOuter = name.find("outer") != std::string::npos;
        bool word = HasWord(name, {"boundary", "boundaries", "bounds", "barrier", "invisiblewall", "invisible wall",
                                   "world border", "worldborder", "world limit", "worldlimit"});
        bool match = outer ? isOuter && (word || HasWord(name, {"wall", "border", "limit"})) : word && !isOuter;
        if (!match) {
            if (seen.size() < 12 && std::find(seen.begin(), seen.end(), name) == seen.end()) seen.push_back(name);
            continue;
        }
        if (!G.behaviourGetEnabledCollider[c]()) continue;
        G.colliderSetEnabled[c](false);
        s.cols[s.n++].set(c);
        if (s.n <= 6) QueueLog("%s: off %s", what, name.c_str());
    }
    if (s.n == 0) {
        QueueLog("%s: nothing matched (%u colliders). Some names:", what, (unsigned)n);
        for (auto &nm : seen) QueueLog("  %s", nm.c_str());
        Notify("%s: nothing found (see the log)", what);
    } else Notify("%s: %d barrier parts off", what, s.n);
}
static void PassTick(PassSet &s, bool want, bool outer, const char *what) {
    if (!want) { if (s.n || s.scanned) PassRelease(s); return; }
    if (!s.scanned && g_time >= s.nextScan) PassScan(s, outer, what);
}

// ---- Fun Mods ------------------------------------------------------------------------
// Punch, Creative, Farther Build and Furthest Build need parts of the game that this menu
// has no names for yet. Switching one on writes what looks related to the log (see
// ProbeGame) and turns it back off, so the real names can be wired in from that log.
static void PunchToggled() { if (f_punch) NotWired("Punch", f_punch, {"punch", "knockback", "fist"}); }
static void CreativeToggled() { if (f_creative) NotWired("Creative", f_creative, {"creative", "blockplace", "placeblock"}); }
static void FarBuildToggled() { if (f_farBuild) NotWired("Farther Build", f_farBuild, {"build", "reach", "terrain"}); }
static void FurthestBuildToggled() { if (f_furthestBuild) NotWired("Furthest Build", f_furthestBuild, {"build", "reach", "terrain"}); }

static Il2CppObject *NetView() {
    Il2CppObject *nm = G.netLocal.IsValid() ? G.netLocal() : nullptr;
    if (!nm || !G.tPhotonView) return nullptr;
    Il2CppObject *go = G.compGameObject[nm]();
    return go ? G.goGetComponent[go]((Il2CppObject *)G.tPhotonView) : nullptr;
}
// The game's "remove an item" message (found by name in Lookups), sent with the item's id.
static bool SendDespawn(const char *id) {
    auto *mi = G.rpcDespawn;
    Il2CppObject *pv = NetView();
    if (!mi || !pv || !G.pvRpc.IsValid() || !G.arrayNew || !G.valueBox) return false;
    int count = mi->parameters_count;
    if (count > 0 && TypeName(mi->parameters[count - 1]) == "Photon.Pun.PhotonMessageInfo") count--;
    auto *args = (Il2CppObject *)G.arrayNew(G.objectClass, count);
    if (!args) return false;
    int strings = 0;
    for (int i = 0; i < count; i++) {
        std::string tn = TypeName(mi->parameters[i]);
        Class pc(mi->parameters[i]);
        auto *k = pc.IsValid() ? pc.GetClass() : nullptr;
        void *v = nullptr;
        if (tn == "System.String") { if (strings++ == 0) v = CreateMonoString(id); }       // the first string is the item id
        else if (k && k->byval_arg.valuetype) {
            uint8_t raw[64] = {};
            auto *boxAs = k->enumtype && k->element_class ? k->element_class : k;
            v = G.valueBox(boxAs, raw);
        }
        Put(args, i, v);
    }
    try { G.pvRpc[pv](CreateMonoString(mi->name), 0, args); }
    catch (...) { ym::log("starter-menu: despawn message was refused"); return false; }
    return true;
}
// Spawn Item: the item in your left hand appears at your position (the room sees it).
static void SpawnHeldItem() {
    Vector3 p;
    if (g_time < g_nextItem) { Notify("Wait a moment"); return; }
    if (g_spawned.size() >= 48) { Notify("Despawn some items first"); return; }
    if (!BodyPos(p) || !LeftHandItem()) return;
    g_nextItem = g_time + 0.5;
    g_itemQueue.push_back({p, Vector3(0, 0, 0), true});
    Notify("Item spawned");
}
// Despawn Items: removes the items made by Spawn Item, a couple a frame.
static void DespawnItems() {
    if (g_spawned.empty()) { Notify("No items to despawn"); return; }
    if (!G.rpcDespawn) {
        Notify("Despawn isn't wired up yet (see the log)");
        ProbeRpcs();
        return;
    }
    for (auto &id : g_spawned) g_despawnQueue.push_back(id);
    g_spawned.clear();
    Notify("Despawning your items");
}

// ---- callbacks ---------------------------------------------------------------

static void OnRoomChange() {
    // The old room's objects are gone (or about to be): forget them, build again.
    for (int r = 0; r < kRows; r++) {
        g_rowCube[r].clear(); g_rowLabel[r].clear(); g_rowText[r].clear();
        g_rowL[r].clear(); g_rowR[r].clear(); g_rowLT[r].clear(); g_rowRT[r].clear();
    }
    for (Ref *r : {&g_board, &g_bg, &g_outline, &g_title, &g_titleBar, &g_prev, &g_next, &g_notifyText, &g_tipText}) r->clear();
    for (Ref *r : {&g_menu, &g_pointer, &g_notifyGo, &g_leftPlatform, &g_rightPlatform, &g_gunBall, &g_gunLineGo, &g_airL, &g_airR, &g_spotCube}) Kill(*r);
    PauseTrackers(false);
    g_dancing = false;
    g_gunLine.clear();
    g_airShown = false; g_airState = -1;
    g_laState = 0; g_laChecks = 0; g_laSpaceL = g_laSpaceR = -1; g_laOwns = false;
    for (PassSet *p : {&g_inner, &g_outer}) {      // the barriers of the old room are gone; look again in the new one
        for (auto &c : p->cols) c.clear();
        p->n = 0; p->scanned = false; p->nextScan = g_time + 3.0;
    }
    g_boosted = nullptr; g_boostApplied = 0;
    g_spawned.clear(); g_despawnQueue.clear();
    g_flyHeld = false; g_velFly = 0;
    for (Ref *r : {&g_player, &g_rigidbody, &g_leftHand, &g_rightHand, &g_head}) r->clear();
    g_built = false; g_menuShown = false; g_touching = -2; g_haveSpot = false;
}

static void OnUpdate(float dt) {
    g_time += dt;
    g_frames++;
    static double fpsStart = 0;
    if (g_time - fpsStart >= 1.0) { g_fps = (float)(g_frames / (g_time - fpsStart)); g_frames = 0; fpsStart = g_time; }
    if (!Lookups()) return;
    FlushLog();
    auto *player = (Il2CppObject *)ym::api()->localPlayer();
    if (!player) return;
    if (player != g_player.get()) {                 // first frame in this room
        OnRoomChange();
        g_player.set(player);
        g_leftHand.set(G.leftHand[player]());
        g_rightHand.set(G.rightHand[player]());
        g_head.set(G.headTransform[player]());
        Il2CppObject *pgo = G.compGameObject[player]();
        if (pgo) g_rigidbody.set(G.goGetComponent[pgo](G.tRigidbody));
    }
    if (!g_leftHand.get() || !g_rightHand.get() || !g_head.get()) return;
    if (!g_built) BuildMenu();

    Buttons in = ReadButtons();
    ShowMenu(f_rightHand ? in.b : in.y);
    if (g_menuShown) {
        PlaceMenu();
        CheckPress();
        if (g_dirty) RenderPage();
        UpdateTitle();
        AnimateMenu();
    }
    if (g_notifyUntil > 0 && g_time >= g_notifyUntil) { Show(g_notifyGo, false); g_notifyUntil = 0; }
    FlightTick(in, dt);
    PlatformFor(in.lgrip, g_leftPlatform, g_leftHand);
    PlatformFor(in.rgrip && !f_tpGun, g_rightPlatform, g_rightHand);   // the right grip aims the Teleport Gun
    ApplySpeedBoost();
    TeleportGunTick(in);
    PassTick(g_inner, f_boundary, false, "Boundary Pass");
    PassTick(g_outer, f_outerBoundary, true, "Outer Boundary Pass");
    try { SpawnOnHandTick(in); ItemQueueTick(); }
    catch (...) { g_itemQueue.clear(); ym::log("starter-menu: item code error"); }
    LongArmsTick();
    DanceTick(in, dt);              // last: it moves your hands after everything else
}

YM_EXPORT int YeepsMod_Init(const YeepsModApi *api) {
    if (!ym::init(api)) return -1;
    ym::onUpdate(OnUpdate);
    ym::log("Blaze Menu loaded on %s (%s)", api->gameVersion, api->buildId);
    return 0;
}
