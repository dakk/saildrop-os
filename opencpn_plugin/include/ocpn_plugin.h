/*
 * OpenCPN Plugin API Header
 * Minimal subset for Saildrop Chart Stream plugin
 *
 * For full development, replace this with the official ocpn_plugin.h from OpenCPN source:
 * https://github.com/OpenCPN/OpenCPN/blob/master/include/ocpn_plugin.h
 */
#ifndef _OCPN_PLUGIN_H_
#define _OCPN_PLUGIN_H_

#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <wx/fileconf.h>

#ifdef __WXMSW__
#define DECL_EXP __declspec(dllexport)
#else
#define DECL_EXP __attribute__((visibility("default")))
#endif

// Plugin capabilities
#define WANTS_OVERLAY_CALLBACK          0x00000001
#define WANTS_CURSOR_LATLON             0x00000002
#define WANTS_TOOLBAR_CALLBACK          0x00000004
#define INSTALLS_TOOLBAR_TOOL           0x00000008
#define WANTS_CONFIG                    0x00000010
#define INSTALLS_TOOLBOX_PAGE           0x00000020
#define INSTALLS_CONTEXTMENU_ITEMS      0x00000040
#define WANTS_NMEA_SENTENCES            0x00000080
#define WANTS_NMEA_EVENTS               0x00000100
#define WANTS_AIS_SENTENCES             0x00000200
#define USES_AUI_MANAGER                0x00000400
#define WANTS_PREFERENCES               0x00000800
#define INSTALLS_PLUGIN_CHART           0x00001000
#define WANTS_ONPAINT_VIEWPORT          0x00002000
#define WANTS_PLUGIN_MESSAGING          0x00004000
#define WANTS_OPENGL_OVERLAY_CALLBACK   0x00008000
#define WANTS_DYNAMIC_OPENGL_OVERLAY_CALLBACK 0x00010000
#define WANTS_LATE_INIT                 0x00020000
#define INSTALLS_PLUGIN_CHART_GL        0x00040000
#define WANTS_MOUSE_EVENTS              0x00080000
#define WANTS_VECTOR_CHART_OBJECT_INFO  0x00100000
#define WANTS_KEYBOARD_EVENTS           0x00200000

// Viewport structure
typedef struct {
    double clat;        // Center latitude
    double clon;        // Center longitude
    double view_scale_ppm;
    double skew;
    double rotation;
    int pix_width;
    int pix_height;
    wxRect rv_rect;
    bool b_quilt;
    int m_projection_type;
    double lat_min;
    double lat_max;
    double lon_min;
    double lon_max;
    bool bValid;
} PlugIn_ViewPort;

// Position fix structures - declared before use
typedef struct {
    double Lat;
    double Lon;
    double Cog;
    double Sog;
    double Var;
    time_t FixTime;
    int nSats;
} PlugIn_Position_Fix;

typedef struct {
    double Lat;
    double Lon;
    double Cog;
    double Sog;
    double Var;
    double Hdm;
    double Hdt;
    time_t FixTime;
    int nSats;
} PlugIn_Position_Fix_Ex;

// Base plugin class
class DECL_EXP opencpn_plugin {
public:
    opencpn_plugin(void* pmgr) {}
    virtual ~opencpn_plugin() {}

    virtual int Init() { return 0; }
    virtual bool DeInit() { return true; }

    virtual int GetAPIVersionMajor() { return 1; }
    virtual int GetAPIVersionMinor() { return 0; }
    virtual int GetPlugInVersionMajor() { return 1; }
    virtual int GetPlugInVersionMinor() { return 0; }

    virtual wxBitmap* GetPlugInBitmap() { return nullptr; }
    virtual wxString GetCommonName() { return wxEmptyString; }
    virtual wxString GetShortDescription() { return wxEmptyString; }
    virtual wxString GetLongDescription() { return wxEmptyString; }

    virtual void SetDefaults() {}
    virtual int GetToolbarToolCount() { return 0; }
    virtual int GetToolboxPanelCount() { return 0; }

    virtual void ShowPreferencesDialog(wxWindow* parent) {}
    virtual void OnToolbarToolCallback(int id) {}
    virtual void SetCursorLatLon(double lat, double lon) {}
    virtual void SetCurrentViewPort(PlugIn_ViewPort& vp) {}
    virtual void SetPositionFix(PlugIn_Position_Fix& pfix) {}

    virtual bool RenderOverlay(wxDC& dc, PlugIn_ViewPort* vp) { return false; }
    virtual void SetNMEASentence(wxString& sentence) {}
    virtual void ProcessParentResize(int x, int y) {}
    virtual void SetColorScheme(int cs) {}
};

// Extended plugin class with OpenGL support
class DECL_EXP opencpn_plugin_18 : public opencpn_plugin {
public:
    opencpn_plugin_18(void* pmgr) : opencpn_plugin(pmgr) {}
    virtual bool RenderGLOverlay(wxGLContext* pcontext, PlugIn_ViewPort* vp) { return false; }
    virtual void SetPluginMessage(wxString& message_id, wxString& message_body) {}
    virtual void SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {}
};

// Latest plugin class
class DECL_EXP opencpn_plugin_118 : public opencpn_plugin_18 {
public:
    opencpn_plugin_118(void* pmgr) : opencpn_plugin_18(pmgr) {}

    // Override to provide plugin capabilities
    virtual int Init() override { return 0; }
    virtual bool DeInit() override { return true; }
};

// API functions - these are provided by OpenCPN at runtime
extern "C" DECL_EXP wxWindow* GetOCPNCanvasWindow();
extern "C" DECL_EXP wxFileConfig* GetOCPNConfigObject();
extern "C" DECL_EXP void AddLocaleCatalog(wxString catalog);
extern "C" DECL_EXP wxString GetPluginDataDir(wxString plugin_name);
extern "C" DECL_EXP int InsertPlugInTool(wxString label, wxBitmap* bitmap, wxBitmap* bmpRollover,
                                         wxItemKind kind, wxString shortHelp, wxString longHelp,
                                         wxObject* clientData, int position, int tool_sel, opencpn_plugin* pplugin);
extern "C" DECL_EXP void SetToolbarItemState(int item, bool toggle);
extern "C" DECL_EXP void RequestRefresh(wxWindow* win);

#endif // _OCPN_PLUGIN_H_
