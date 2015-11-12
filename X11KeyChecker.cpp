#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdio>
#include "X11KeyChecker.h"
#include "X11KeyChecker.xpm"
#include "X11KeyCheckerMask.xbm"
using namespace std;

// proram name
#define PROGNAME    "X11KeyChecker"
static char progname[] = PROGNAME;

// size of window
#define WIN_WIDTH   200
#define WIN_HEIGHT  100

// border width
#define BORDER_WIDTH 1

// the application
struct X11App {
    int             m_argc;             // number of command line paramters
    char **         m_argv;             // command line paramters

    Display *       m_disp;             // X11 display
    Window          m_root_win;         // the root window
    Window          m_win;              // the main window
    GC              m_gc1;              // graphic context
    GC              m_gc2;              // graphic context
    GC              m_gc3;              // graphic context
    bool            m_quit;             // quit flag
    Atom            m_wm_delete_window; // WM_DELETE_WINDOW atom

    bool            m_shift_pressed;    // Is [Shift] key pressed?
    bool            m_ctrl_pressed;     // Is [Ctrl] key pressed?

    Pixmap          m_icon_pixmap;      // icon pixmap
    Pixmap          m_icon_mask;        // icon mask

    std::string     m_key_name;

    X11App(int argc, char **argv) : m_argc(argc), m_argv(argv) {
        m_quit = false;
        m_shift_pressed = false;
        m_ctrl_pressed = false;
    }

    ~X11App() {
        // free icon pixmap
        if (m_icon_pixmap != None) {
            XFreePixmap(m_disp, m_icon_pixmap);
        }
        if (m_icon_mask != None) {
            XFreePixmap(m_disp, m_icon_mask);
        }
        // free graphic contexts
        XFreeGC(m_disp, m_gc1);
        XFreeGC(m_disp, m_gc2);
        XFreeGC(m_disp, m_gc3);
        // close display
        XCloseDisplay(m_disp);
    }

    bool load_icon_pixmap() {
        m_icon_pixmap = None;
        int status = XpmCreatePixmapFromData(
            m_disp, m_root_win, (char **)X11KeyChecker_xpm,
            &m_icon_pixmap,
            NULL, NULL);
        m_icon_mask = XCreatePixmapFromBitmapData(
            m_disp, m_root_win, (char *)X11KeyCheckerMask_bits,
            X11KeyCheckerMask_width, X11KeyCheckerMask_height,
            WhitePixel(m_disp, 0), BlackPixel(m_disp, 0),
            1);
        return status == 0;
    }

    void set_standard_properties() {
        XSizeHints *psize_hints = XAllocSizeHints();
        psize_hints->flags = PMinSize | PMaxSize;
        psize_hints->min_width = WIN_WIDTH;
        psize_hints->min_height = WIN_HEIGHT;
        psize_hints->max_width = WIN_WIDTH;
        psize_hints->max_height = WIN_HEIGHT;
        XSetStandardProperties(m_disp, m_win, progname, progname,
            m_icon_pixmap, m_argv, m_argc, psize_hints);
        XFree(psize_hints);

        XWMHints *hints = XAllocWMHints();
        hints->flags = IconPixmapHint | IconMaskHint;
        hints->icon_pixmap = m_icon_pixmap;
        hints->icon_mask = m_icon_mask;
        XSetWMHints(m_disp, m_win, hints);
        XFree(hints);
    } // set_standard_properties

    bool startup() {
        // open display
        m_disp = XOpenDisplay(NULL);
        if (m_disp != NULL) {
            // get the root window
            m_root_win = RootWindow(m_disp, 0);

            // create the main window
            m_win = XCreateSimpleWindow(m_disp, m_root_win,
                0, 0, WIN_WIDTH, WIN_HEIGHT, BORDER_WIDTH,
                WhitePixel(m_disp, 0),
                BlackPixel(m_disp, 0)
            );
            if (m_win != 0) {
                // set event masks
                XSelectInput(m_disp, m_win,
                    ExposureMask |
                    ButtonPressMask | ButtonReleaseMask | ButtonMotionMask |
                    KeyPressMask | KeyReleaseMask |
                    StructureNotifyMask);

                // create graphic contexts
                m_gc1 = XCreateGC(m_disp, m_win, 0, NULL);
                m_gc2 = XCreateGC(m_disp, m_win, 0, NULL);
                m_gc3 = XCreateGC(m_disp, m_win, 0, NULL);

                // set WM_DELETE_WINDOW protocol
                m_wm_delete_window = XInternAtom(m_disp, "WM_DELETE_WINDOW", False);
                XSetWMProtocols(m_disp, m_win, &m_wm_delete_window, 1);

                // load icon pixmap
                load_icon_pixmap();
                
                // set standard properties
                set_standard_properties();

                // show the window
                XMapWindow(m_disp, m_win);
                return true;
            } else {
                fprintf(stderr, PROGNAME ": ERROR: XCreateSimpleWindow fails\n");
            }
        }
        return false;
    }

    int run() {
        // event loop
        XEvent e;
        while (!m_quit) {
            if (XPending(m_disp)) {
                XLockDisplay(m_disp);   // multithread support
                XNextEvent(m_disp, &e);
                XUnlockDisplay(m_disp); // multithread support
                switch (e.type) {
                case Expose:
                    onExpose(&e.xexpose);
                    break;
                case ButtonPress:
                    onButtonPress(&e.xbutton);
                    break;
                case ButtonRelease:
                    onButtonRelease(&e.xbutton);
                    break;
                case MotionNotify:
                    onMotionNotify(&e.xmotion);
                    break;
                case KeyPress:
                    onKeyPress(&e.xkey);
                    break;
                case KeyRelease:
                    onKeyRelease(&e.xkey);
                    break;
                case ClientMessage:
                    onClientMessage(&e.xclient);
                    break;
                case DestroyNotify:
                    onDestroyNotify(&e.xdestroywindow);
                    break;
                case MappingNotify:
                    onMappingNotify(&e.xmapping);
                    break;
                }
            } else {
                onIdle();
            }
        }
        return 0;
    }

    //
    // event handlers
    //
    void onExpose(XExposeEvent *pee) {
        // get width and height
        unsigned int width, height;
        {
            Window root;
            int x, y;
            unsigned int bwidth, depth;
            XGetGeometry(m_disp, m_win, &root, &x, &y, &width, &height,
                &bwidth, &depth);
        }

        XSetForeground(m_disp, m_gc1, WhitePixel(m_disp, 0));
        XSetBackground(m_disp, m_gc1, BlackPixel(m_disp, 0));

        // clear window
        XClearWindow(m_disp, m_win);

        // draw string
        Font font = XLoadFont(m_disp, "f*");
        XSetFont(m_disp, m_gc1, font);
        XFontStruct *fs = XQueryFont(m_disp, font);
        {
            auto text = m_key_name.c_str();
            int text_len = int(m_key_name.size());

            int width = XTextWidth(fs, text, text_len);

            int dir, ascent, descent;
            XCharStruct cs;
            XQueryTextExtents(m_disp, font, text, text_len,
                &dir, &ascent, &descent, &cs);
            int height = cs.ascent + cs.descent;
            XDrawString(m_disp, m_win, m_gc1,
                (WIN_WIDTH - width) / 2,
                (WIN_HEIGHT + height) / 2,
                text, text_len);
        }
        XFreeFontInfo(NULL, fs, 0);
        XUnloadFont(m_disp, font);
    }
    void onButtonPress(XButtonEvent *pbe) {
        //printf("pressed button: %d, x: %d, y: %d\n", pbe->button, pbe->x, pbe->y);
    }
    void onButtonRelease(XButtonEvent *pbe) {
        //printf("released button: %d, x: %d, y: %d\n", pbe->button, pbe->x, pbe->y);
    }
    void onMotionNotify(XMotionEvent *pme) {
        ;
    }
    std::string get_key_name(KeySym ks) const {
        switch (ks) {
        case XK_BackSpace: return "XK_BackSpace";
        case XK_Tab: return "XK_Tab";
        case XK_Linefeed: return "XK_Linefeed";
        case XK_Clear: return "XK_Clear";
        case XK_Return: return "XK_Return";
        case XK_Pause: return "XK_Pause";
        case XK_Scroll_Lock: return "XK_Scroll_Lock";
        case XK_Sys_Req: return "XK_Sys_Req";
        case XK_Escape: return "XK_Escape";
        case XK_Delete: return "XK_Delete";
        case XK_Multi_key: return "XK_Multi_key";
        case XK_Codeinput: return "XK_Codeinput";
        case XK_SingleCandidate: return "XK_SingleCandidate";
        case XK_MultipleCandidate: return "XK_MultipleCandidate";
        case XK_PreviousCandidate: return "XK_PreviousCandidate";
        case XK_Kanji: return "XK_Kanji";
        case XK_Muhenkan: return "XK_Muhenkan";
        //case XK_Henkan_Mode: return "XK_Henkan_Mode";
        case XK_Henkan: return "XK_Henkan";
        case XK_Romaji: return "XK_Romaji";
        case XK_Hiragana: return "XK_Hiragana";
        case XK_Katakana: return "XK_Katakana";
        case XK_Hiragana_Katakana: return "XK_Hiragana_Katakana";
        case XK_Zenkaku: return "XK_Zenkaku";
        case XK_Hankaku: return "XK_Hankaku";
        case XK_Zenkaku_Hankaku: return "XK_Zenkaku_Hankaku";
        case XK_Touroku: return "XK_Touroku";
        case XK_Massyo: return "XK_Massyo";
        case XK_Kana_Lock: return "XK_Kana_Lock";
        case XK_Kana_Shift: return "XK_Kana_Shift";
        case XK_Eisu_Shift: return "XK_Eisu_Shift";
        case XK_Eisu_toggle: return "XK_Eisu_toggle";
        //case XK_Kanji_Bangou: return "XK_Kanji_Bangou";
        //case XK_Zen_Koho: return "XK_Zen_Koho";
        //case XK_Mae_Koho: return "XK_Mae_Koho";
        case XK_Home: return "XK_Home";
        case XK_Left: return "XK_Left";
        case XK_Up: return "XK_Up";
        case XK_Right: return "XK_Right";
        case XK_Down: return "XK_Down";
        //case XK_Prior: return "XK_Prior";
        case XK_Page_Up: return "XK_Page_Up";
        //case XK_Next: return "XK_Next";
        case XK_Page_Down: return "XK_Page_Down";
        case XK_End: return "XK_End";
        case XK_Begin: return "XK_Begin";
        case XK_Select: return "XK_Select";
        case XK_Print: return "XK_Print";
        case XK_Execute: return "XK_Execute";
        case XK_Insert: return "XK_Insert";
        case XK_Undo: return "XK_Undo";
        case XK_Redo: return "XK_Redo";
        case XK_Menu: return "XK_Menu";
        case XK_Find: return "XK_Find";
        case XK_Cancel: return "XK_Cancel";
        case XK_Help: return "XK_Help";
        case XK_Break: return "XK_Break";
        case XK_Mode_switch: return "XK_Mode_switch";
        //case XK_script_switch: return "XK_script_switch";
        case XK_Num_Lock: return "XK_Num_Lock";
        case XK_KP_Space: return "XK_KP_Space";
        case XK_KP_Tab: return "XK_KP_Tab";
        case XK_KP_Enter: return "XK_KP_Enter";
        case XK_KP_F1: return "XK_KP_F1";
        case XK_KP_F2: return "XK_KP_F2";
        case XK_KP_F3: return "XK_KP_F3";
        case XK_KP_F4: return "XK_KP_F4";
        case XK_KP_Home: return "XK_KP_Home";
        case XK_KP_Left: return "XK_KP_Left";
        case XK_KP_Up: return "XK_KP_Up";
        case XK_KP_Right: return "XK_KP_Right";
        case XK_KP_Down: return "XK_KP_Down";
        //case XK_KP_Prior: return "XK_KP_Prior";
        case XK_KP_Page_Up: return "XK_KP_Page_Up";
        //case XK_KP_Next: return "XK_KP_Next";
        case XK_KP_Page_Down: return "XK_KP_Page_Down";
        case XK_KP_End: return "XK_KP_End";
        case XK_KP_Begin: return "XK_KP_Begin";
        case XK_KP_Insert: return "XK_KP_Insert";
        case XK_KP_Delete: return "XK_KP_Delete";
        case XK_KP_Equal: return "XK_KP_Equal";
        case XK_KP_Multiply: return "XK_KP_Multiply";
        case XK_KP_Add: return "XK_KP_Add";
        case XK_KP_Separator: return "XK_KP_Separator";
        case XK_KP_Subtract: return "XK_KP_Subtract";
        case XK_KP_Decimal: return "XK_KP_Decimal";
        case XK_KP_Divide: return "XK_KP_Divide";
        case XK_KP_0: return "XK_KP_0";
        case XK_KP_1: return "XK_KP_1";
        case XK_KP_2: return "XK_KP_2";
        case XK_KP_3: return "XK_KP_3";
        case XK_KP_4: return "XK_KP_4";
        case XK_KP_5: return "XK_KP_5";
        case XK_KP_6: return "XK_KP_6";
        case XK_KP_7: return "XK_KP_7";
        case XK_KP_8: return "XK_KP_8";
        case XK_KP_9: return "XK_KP_9";
        case XK_F1: return "XK_F1";
        case XK_F2: return "XK_F2";
        case XK_F3: return "XK_F3";
        case XK_F4: return "XK_F4";
        case XK_F5: return "XK_F5";
        case XK_F6: return "XK_F6";
        case XK_F7: return "XK_F7";
        case XK_F8: return "XK_F8";
        case XK_F9: return "XK_F9";
        case XK_F10: return "XK_F10";
        case XK_F11: return "XK_F11";
        //case XK_L1: return "XK_L1";
        case XK_F12: return "XK_F12";
        //case XK_L2: return "XK_L2";
        case XK_F13: return "XK_F13";
        //case XK_L3: return "XK_L3";
        case XK_F14: return "XK_F14";
        //case XK_L4: return "XK_L4";
        case XK_F15: return "XK_F15";
        //case XK_L5: return "XK_L5";
        case XK_F16: return "XK_F16";
        //case XK_L6: return "XK_L6";
        case XK_F17: return "XK_F17";
        //case XK_L7: return "XK_L7";
        case XK_F18: return "XK_F18";
        //case XK_L8: return "XK_L8";
        case XK_F19: return "XK_F19";
        //case XK_L9: return "XK_L9";
        case XK_F20: return "XK_F20";
        //case XK_L10: return "XK_L10";
        case XK_F21: return "XK_F21";
        //case XK_R1: return "XK_R1";
        case XK_F22: return "XK_F22";
        //case XK_R2: return "XK_R2";
        case XK_F23: return "XK_F23";
        //case XK_R3: return "XK_R3";
        case XK_F24: return "XK_F24";
        //case XK_R4: return "XK_R4";
        case XK_F25: return "XK_F25";
        //case XK_R5: return "XK_R5";
        case XK_F26: return "XK_F26";
        //case XK_R6: return "XK_R6";
        case XK_F27: return "XK_F27";
        //case XK_R7: return "XK_R7";
        case XK_F28: return "XK_F28";
        //case XK_R8: return "XK_R8";
        case XK_F29: return "XK_F29";
        //case XK_R9: return "XK_R9";
        case XK_F30: return "XK_F30";
        //case XK_R10: return "XK_R10";
        case XK_F31: return "XK_F31";
        //case XK_R11: return "XK_R11";
        case XK_F32: return "XK_F32";
        //case XK_R12: return "XK_R12";
        case XK_F33: return "XK_F33";
        //case XK_R13: return "XK_R13";
        case XK_F34: return "XK_F34";
        //case XK_R14: return "XK_R14";
        case XK_F35: return "XK_F35";
        //case XK_R15: return "XK_R15";
        case XK_Shift_L: return "XK_Shift_L";
        case XK_Shift_R: return "XK_Shift_R";
        case XK_Control_L: return "XK_Control_L";
        case XK_Control_R: return "XK_Control_R";
        case XK_Caps_Lock: return "XK_Caps_Lock";
        case XK_Shift_Lock: return "XK_Shift_Lock";
        case XK_Meta_L: return "XK_Meta_L";
        case XK_Meta_R: return "XK_Meta_R";
        case XK_Alt_L: return "XK_Alt_L";
        case XK_Alt_R: return "XK_Alt_R";
        case XK_Super_L: return "XK_Super_L";
        case XK_Super_R: return "XK_Super_R";
        case XK_Hyper_L: return "XK_Hyper_L";
        case XK_Hyper_R: return "XK_Hyper_R";
        case XK_space: return "XK_space";
        case XK_exclam: return "XK_exclam";
        case XK_quotedbl: return "XK_quotedbl";
        case XK_numbersign: return "XK_numbersign";
        case XK_dollar: return "XK_dollar";
        case XK_percent: return "XK_percent";
        case XK_ampersand: return "XK_ampersand";
        case XK_apostrophe: return "XK_apostrophe";
        //case XK_quoteright: return "XK_quoteright";
        case XK_parenleft: return "XK_parenleft";
        case XK_parenright: return "XK_parenright";
        case XK_asterisk: return "XK_asterisk";
        case XK_plus: return "XK_plus";
        case XK_comma: return "XK_comma";
        case XK_minus: return "XK_minus";
        case XK_period: return "XK_period";
        case XK_slash: return "XK_slash";
        case XK_0: return "XK_0";
        case XK_1: return "XK_1";
        case XK_2: return "XK_2";
        case XK_3: return "XK_3";
        case XK_4: return "XK_4";
        case XK_5: return "XK_5";
        case XK_6: return "XK_6";
        case XK_7: return "XK_7";
        case XK_8: return "XK_8";
        case XK_9: return "XK_9";
        case XK_colon: return "XK_colon";
        case XK_semicolon: return "XK_semicolon";
        case XK_less: return "XK_less";
        case XK_equal: return "XK_equal";
        case XK_greater: return "XK_greater";
        case XK_question: return "XK_question";
        case XK_at: return "XK_at";
        case XK_A: return "XK_A";
        case XK_B: return "XK_B";
        case XK_C: return "XK_C";
        case XK_D: return "XK_D";
        case XK_E: return "XK_E";
        case XK_F: return "XK_F";
        case XK_G: return "XK_G";
        case XK_H: return "XK_H";
        case XK_I: return "XK_I";
        case XK_J: return "XK_J";
        case XK_K: return "XK_K";
        case XK_L: return "XK_L";
        case XK_M: return "XK_M";
        case XK_N: return "XK_N";
        case XK_O: return "XK_O";
        case XK_P: return "XK_P";
        case XK_Q: return "XK_Q";
        case XK_R: return "XK_R";
        case XK_S: return "XK_S";
        case XK_T: return "XK_T";
        case XK_U: return "XK_U";
        case XK_V: return "XK_V";
        case XK_W: return "XK_W";
        case XK_X: return "XK_X";
        case XK_Y: return "XK_Y";
        case XK_Z: return "XK_Z";
        case XK_bracketleft: return "XK_bracketleft";
        case XK_backslash: return "XK_backslash";
        case XK_bracketright: return "XK_bracketright";
        case XK_asciicircum: return "XK_asciicircum";
        case XK_underscore: return "XK_underscore";
        case XK_grave: return "XK_grave";
        //case XK_quoteleft: return "XK_quoteleft";
        case XK_a: return "XK_a";
        case XK_b: return "XK_b";
        case XK_c: return "XK_c";
        case XK_d: return "XK_d";
        case XK_e: return "XK_e";
        case XK_f: return "XK_f";
        case XK_g: return "XK_g";
        case XK_h: return "XK_h";
        case XK_i: return "XK_i";
        case XK_j: return "XK_j";
        case XK_k: return "XK_k";
        case XK_l: return "XK_l";
        case XK_m: return "XK_m";
        case XK_n: return "XK_n";
        case XK_o: return "XK_o";
        case XK_p: return "XK_p";
        case XK_q: return "XK_q";
        case XK_r: return "XK_r";
        case XK_s: return "XK_s";
        case XK_t: return "XK_t";
        case XK_u: return "XK_u";
        case XK_v: return "XK_v";
        case XK_w: return "XK_w";
        case XK_x: return "XK_x";
        case XK_y: return "XK_y";
        case XK_z: return "XK_z";
        case XK_braceleft: return "XK_braceleft";
        case XK_bar: return "XK_bar";
        case XK_braceright: return "XK_braceright";
        case XK_asciitilde: return "XK_asciitilde";
        case XK_nobreakspace: return "XK_nobreakspace";
        case XK_exclamdown: return "XK_exclamdown";
        case XK_cent: return "XK_cent";
        case XK_sterling: return "XK_sterling";
        case XK_currency: return "XK_currency";
        case XK_yen: return "XK_yen";
        case XK_brokenbar: return "XK_brokenbar";
        case XK_section: return "XK_section";
        case XK_diaeresis: return "XK_diaeresis";
        case XK_copyright: return "XK_copyright";
        case XK_ordfeminine: return "XK_ordfeminine";
        case XK_guillemotleft: return "XK_guillemotleft";
        case XK_notsign: return "XK_notsign";
        case XK_hyphen: return "XK_hyphen";
        case XK_registered: return "XK_registered";
        case XK_macron: return "XK_macron";
        case XK_degree: return "XK_degree";
        case XK_plusminus: return "XK_plusminus";
        case XK_twosuperior: return "XK_twosuperior";
        case XK_threesuperior: return "XK_threesuperior";
        case XK_acute: return "XK_acute";
        case XK_mu: return "XK_mu";
        case XK_paragraph: return "XK_paragraph";
        case XK_periodcentered: return "XK_periodcentered";
        case XK_cedilla: return "XK_cedilla";
        case XK_onesuperior: return "XK_onesuperior";
        case XK_masculine: return "XK_masculine";
        case XK_guillemotright: return "XK_guillemotright";
        case XK_onequarter: return "XK_onequarter";
        case XK_onehalf: return "XK_onehalf";
        case XK_threequarters: return "XK_threequarters";
        case XK_questiondown: return "XK_questiondown";
        case XK_Agrave: return "XK_Agrave";
        case XK_Aacute: return "XK_Aacute";
        case XK_Acircumflex: return "XK_Acircumflex";
        case XK_Atilde: return "XK_Atilde";
        case XK_Adiaeresis: return "XK_Adiaeresis";
        case XK_Aring: return "XK_Aring";
        case XK_AE: return "XK_AE";
        case XK_Ccedilla: return "XK_Ccedilla";
        case XK_Egrave: return "XK_Egrave";
        case XK_Eacute: return "XK_Eacute";
        case XK_Ecircumflex: return "XK_Ecircumflex";
        case XK_Ediaeresis: return "XK_Ediaeresis";
        case XK_Igrave: return "XK_Igrave";
        case XK_Iacute: return "XK_Iacute";
        case XK_Icircumflex: return "XK_Icircumflex";
        case XK_Idiaeresis: return "XK_Idiaeresis";
        //case XK_ETH: return "XK_ETH";
        case XK_Eth: return "XK_Eth";
        case XK_Ntilde: return "XK_Ntilde";
        case XK_Ograve: return "XK_Ograve";
        case XK_Oacute: return "XK_Oacute";
        case XK_Ocircumflex: return "XK_Ocircumflex";
        case XK_Otilde: return "XK_Otilde";
        case XK_Odiaeresis: return "XK_Odiaeresis";
        case XK_multiply: return "XK_multiply";
        case XK_Oslash: return "XK_Oslash";
        //case XK_Ooblique: return "XK_Ooblique";
        case XK_Ugrave: return "XK_Ugrave";
        case XK_Uacute: return "XK_Uacute";
        case XK_Ucircumflex: return "XK_Ucircumflex";
        case XK_Udiaeresis: return "XK_Udiaeresis";
        case XK_Yacute: return "XK_Yacute";
        //case XK_THORN: return "XK_THORN";
        case XK_Thorn: return "XK_Thorn";
        case XK_ssharp: return "XK_ssharp";
        case XK_agrave: return "XK_agrave";
        case XK_aacute: return "XK_aacute";
        case XK_acircumflex: return "XK_acircumflex";
        case XK_atilde: return "XK_atilde";
        case XK_adiaeresis: return "XK_adiaeresis";
        case XK_aring: return "XK_aring";
        case XK_ae: return "XK_ae";
        case XK_ccedilla: return "XK_ccedilla";
        case XK_egrave: return "XK_egrave";
        case XK_eacute: return "XK_eacute";
        case XK_ecircumflex: return "XK_ecircumflex";
        case XK_ediaeresis: return "XK_ediaeresis";
        case XK_igrave: return "XK_igrave";
        case XK_iacute: return "XK_iacute";
        case XK_icircumflex: return "XK_icircumflex";
        case XK_idiaeresis: return "XK_idiaeresis";
        case XK_eth: return "XK_eth";
        case XK_ntilde: return "XK_ntilde";
        case XK_ograve: return "XK_ograve";
        case XK_oacute: return "XK_oacute";
        case XK_ocircumflex: return "XK_ocircumflex";
        case XK_otilde: return "XK_otilde";
        case XK_odiaeresis: return "XK_odiaeresis";
        case XK_division: return "XK_division";
        case XK_oslash: return "XK_oslash";
        //case XK_ooblique: return "XK_ooblique";
        case XK_ugrave: return "XK_ugrave";
        case XK_uacute: return "XK_uacute";
        case XK_ucircumflex: return "XK_ucircumflex";
        case XK_udiaeresis: return "XK_udiaeresis";
        case XK_yacute: return "XK_yacute";
        case XK_thorn: return "XK_thorn";
        case XK_ydiaeresis: return "XK_ydiaeresis";
        case XK_Aogonek: return "XK_Aogonek";
        case XK_breve: return "XK_breve";
        case XK_Lstroke: return "XK_Lstroke";
        case XK_Lcaron: return "XK_Lcaron";
        case XK_Sacute: return "XK_Sacute";
        case XK_Scaron: return "XK_Scaron";
        case XK_Scedilla: return "XK_Scedilla";
        case XK_Tcaron: return "XK_Tcaron";
        case XK_Zacute: return "XK_Zacute";
        case XK_Zcaron: return "XK_Zcaron";
        case XK_Zabovedot: return "XK_Zabovedot";
        case XK_aogonek: return "XK_aogonek";
        case XK_ogonek: return "XK_ogonek";
        case XK_lstroke: return "XK_lstroke";
        case XK_lcaron: return "XK_lcaron";
        case XK_sacute: return "XK_sacute";
        case XK_caron: return "XK_caron";
        case XK_scaron: return "XK_scaron";
        case XK_scedilla: return "XK_scedilla";
        case XK_tcaron: return "XK_tcaron";
        case XK_zacute: return "XK_zacute";
        case XK_doubleacute: return "XK_doubleacute";
        case XK_zcaron: return "XK_zcaron";
        case XK_zabovedot: return "XK_zabovedot";
        case XK_Racute: return "XK_Racute";
        case XK_Abreve: return "XK_Abreve";
        case XK_Lacute: return "XK_Lacute";
        case XK_Cacute: return "XK_Cacute";
        case XK_Ccaron: return "XK_Ccaron";
        case XK_Eogonek: return "XK_Eogonek";
        case XK_Ecaron: return "XK_Ecaron";
        case XK_Dcaron: return "XK_Dcaron";
        case XK_Dstroke: return "XK_Dstroke";
        case XK_Nacute: return "XK_Nacute";
        case XK_Ncaron: return "XK_Ncaron";
        case XK_Odoubleacute: return "XK_Odoubleacute";
        case XK_Rcaron: return "XK_Rcaron";
        case XK_Uring: return "XK_Uring";
        case XK_Udoubleacute: return "XK_Udoubleacute";
        case XK_Tcedilla: return "XK_Tcedilla";
        case XK_racute: return "XK_racute";
        case XK_abreve: return "XK_abreve";
        case XK_lacute: return "XK_lacute";
        case XK_cacute: return "XK_cacute";
        case XK_ccaron: return "XK_ccaron";
        case XK_eogonek: return "XK_eogonek";
        case XK_ecaron: return "XK_ecaron";
        case XK_dcaron: return "XK_dcaron";
        case XK_dstroke: return "XK_dstroke";
        case XK_nacute: return "XK_nacute";
        case XK_ncaron: return "XK_ncaron";
        case XK_odoubleacute: return "XK_odoubleacute";
        case XK_rcaron: return "XK_rcaron";
        case XK_uring: return "XK_uring";
        case XK_udoubleacute: return "XK_udoubleacute";
        case XK_tcedilla: return "XK_tcedilla";
        case XK_abovedot: return "XK_abovedot";
        case XK_Hstroke: return "XK_Hstroke";
        case XK_Hcircumflex: return "XK_Hcircumflex";
        case XK_Iabovedot: return "XK_Iabovedot";
        case XK_Gbreve: return "XK_Gbreve";
        case XK_Jcircumflex: return "XK_Jcircumflex";
        case XK_hstroke: return "XK_hstroke";
        case XK_hcircumflex: return "XK_hcircumflex";
        case XK_idotless: return "XK_idotless";
        case XK_gbreve: return "XK_gbreve";
        case XK_jcircumflex: return "XK_jcircumflex";
        case XK_Cabovedot: return "XK_Cabovedot";
        case XK_Ccircumflex: return "XK_Ccircumflex";
        case XK_Gabovedot: return "XK_Gabovedot";
        case XK_Gcircumflex: return "XK_Gcircumflex";
        case XK_Ubreve: return "XK_Ubreve";
        case XK_Scircumflex: return "XK_Scircumflex";
        case XK_cabovedot: return "XK_cabovedot";
        case XK_ccircumflex: return "XK_ccircumflex";
        case XK_gabovedot: return "XK_gabovedot";
        case XK_gcircumflex: return "XK_gcircumflex";
        case XK_ubreve: return "XK_ubreve";
        case XK_scircumflex: return "XK_scircumflex";
        //case XK_kra: return "XK_kra";
        case XK_kappa: return "XK_kappa";
        case XK_Rcedilla: return "XK_Rcedilla";
        case XK_Itilde: return "XK_Itilde";
        case XK_Lcedilla: return "XK_Lcedilla";
        case XK_Emacron: return "XK_Emacron";
        case XK_Gcedilla: return "XK_Gcedilla";
        case XK_Tslash: return "XK_Tslash";
        case XK_rcedilla: return "XK_rcedilla";
        case XK_itilde: return "XK_itilde";
        case XK_lcedilla: return "XK_lcedilla";
        case XK_emacron: return "XK_emacron";
        case XK_gcedilla: return "XK_gcedilla";
        case XK_tslash: return "XK_tslash";
        case XK_ENG: return "XK_ENG";
        case XK_eng: return "XK_eng";
        case XK_Amacron: return "XK_Amacron";
        case XK_Iogonek: return "XK_Iogonek";
        case XK_Eabovedot: return "XK_Eabovedot";
        case XK_Imacron: return "XK_Imacron";
        case XK_Ncedilla: return "XK_Ncedilla";
        case XK_Omacron: return "XK_Omacron";
        case XK_Kcedilla: return "XK_Kcedilla";
        case XK_Uogonek: return "XK_Uogonek";
        case XK_Utilde: return "XK_Utilde";
        case XK_Umacron: return "XK_Umacron";
        case XK_amacron: return "XK_amacron";
        case XK_iogonek: return "XK_iogonek";
        case XK_eabovedot: return "XK_eabovedot";
        case XK_imacron: return "XK_imacron";
        case XK_ncedilla: return "XK_ncedilla";
        case XK_omacron: return "XK_omacron";
        case XK_kcedilla: return "XK_kcedilla";
        case XK_uogonek: return "XK_uogonek";
        case XK_utilde: return "XK_utilde";
        case XK_umacron: return "XK_umacron";
        case XK_OE: return "XK_OE";
        case XK_oe: return "XK_oe";
        case XK_Ydiaeresis: return "XK_Ydiaeresis";
        case XK_overline: return "XK_overline";
        case XK_kana_fullstop: return "XK_kana_fullstop";
        case XK_kana_openingbracket: return "XK_kana_openingbracket";
        case XK_kana_closingbracket: return "XK_kana_closingbracket";
        case XK_kana_comma: return "XK_kana_comma";
        //case XK_kana_conjunctive: return "XK_kana_conjunctive";
        case XK_kana_middledot: return "XK_kana_middledot";
        case XK_kana_WO: return "XK_kana_WO";
        case XK_kana_a: return "XK_kana_a";
        case XK_kana_i: return "XK_kana_i";
        case XK_kana_u: return "XK_kana_u";
        case XK_kana_e: return "XK_kana_e";
        case XK_kana_o: return "XK_kana_o";
        case XK_kana_ya: return "XK_kana_ya";
        case XK_kana_yu: return "XK_kana_yu";
        case XK_kana_yo: return "XK_kana_yo";
        case XK_kana_tsu: return "XK_kana_tsu";
        //case XK_kana_tu: return "XK_kana_tu";
        case XK_prolongedsound: return "XK_prolongedsound";
        case XK_kana_A: return "XK_kana_A";
        case XK_kana_I: return "XK_kana_I";
        case XK_kana_U: return "XK_kana_U";
        case XK_kana_E: return "XK_kana_E";
        case XK_kana_O: return "XK_kana_O";
        case XK_kana_KA: return "XK_kana_KA";
        case XK_kana_KI: return "XK_kana_KI";
        case XK_kana_KU: return "XK_kana_KU";
        case XK_kana_KE: return "XK_kana_KE";
        case XK_kana_KO: return "XK_kana_KO";
        case XK_kana_SA: return "XK_kana_SA";
        case XK_kana_SHI: return "XK_kana_SHI";
        case XK_kana_SU: return "XK_kana_SU";
        case XK_kana_SE: return "XK_kana_SE";
        case XK_kana_SO: return "XK_kana_SO";
        case XK_kana_TA: return "XK_kana_TA";
        case XK_kana_CHI: return "XK_kana_CHI";
        //case XK_kana_TI: return "XK_kana_TI";
        case XK_kana_TSU: return "XK_kana_TSU";
        //case XK_kana_TU: return "XK_kana_TU";
        case XK_kana_TE: return "XK_kana_TE";
        case XK_kana_TO: return "XK_kana_TO";
        case XK_kana_NA: return "XK_kana_NA";
        case XK_kana_NI: return "XK_kana_NI";
        case XK_kana_NU: return "XK_kana_NU";
        case XK_kana_NE: return "XK_kana_NE";
        case XK_kana_NO: return "XK_kana_NO";
        case XK_kana_HA: return "XK_kana_HA";
        case XK_kana_HI: return "XK_kana_HI";
        case XK_kana_FU: return "XK_kana_FU";
        //case XK_kana_HU: return "XK_kana_HU";
        case XK_kana_HE: return "XK_kana_HE";
        case XK_kana_HO: return "XK_kana_HO";
        case XK_kana_MA: return "XK_kana_MA";
        case XK_kana_MI: return "XK_kana_MI";
        case XK_kana_MU: return "XK_kana_MU";
        case XK_kana_ME: return "XK_kana_ME";
        case XK_kana_MO: return "XK_kana_MO";
        case XK_kana_YA: return "XK_kana_YA";
        case XK_kana_YU: return "XK_kana_YU";
        case XK_kana_YO: return "XK_kana_YO";
        case XK_kana_RA: return "XK_kana_RA";
        case XK_kana_RI: return "XK_kana_RI";
        case XK_kana_RU: return "XK_kana_RU";
        case XK_kana_RE: return "XK_kana_RE";
        case XK_kana_RO: return "XK_kana_RO";
        case XK_kana_WA: return "XK_kana_WA";
        case XK_kana_N: return "XK_kana_N";
        case XK_voicedsound: return "XK_voicedsound";
        case XK_semivoicedsound: return "XK_semivoicedsound";
        //case XK_kana_switch: return "XK_kana_switch";
        default: return "(unknown)";
        }
    }
    void onKeyPress(XKeyEvent *pke) {
        // See <X11/keysymdef.h>
        KeySym ks;
        ks = XLookupKeysym(pke, 0);
        m_key_name = get_key_name(ks);
        switch (ks)
        {
        case XK_Shift_L:
        case XK_Shift_R:
            m_shift_pressed = true;
            //puts("Shift key pressed");
            break;
        case XK_Control_L:
        case XK_Control_R:
            m_ctrl_pressed = true;
            //puts("Ctrl key pressed");
            break;
        default:
            break;
        }
        // redraw
        XClearArea(m_disp, m_win, 0, 0, WIN_WIDTH, WIN_HEIGHT, True);
    }
    void onKeyRelease(XKeyEvent *pke) {
        // See <X11/keysymdef.h>
        KeySym ks;
        ks = XLookupKeysym(pke, 0);
        switch (ks)
        {
        case XK_Shift_L:
        case XK_Shift_R:
            m_shift_pressed = false;
            //puts("Shift key released");
            break;
        case XK_Control_L:
        case XK_Control_R:
            m_ctrl_pressed = false;
            //puts("Ctrl key released");
            break;
        default:
            break;
        }
    }
    void onMappingNotify(XMappingEvent *pme) {
        XRefreshKeyboardMapping(pme);
    }
    void onIdle() {
        ;
    }
    void onClientMessage(XClientMessageEvent *pcme) {
        if ((Atom)pcme->data.l[0] == m_wm_delete_window) {
            XDestroyWindow(m_disp, m_win);
        }
    }
    void onDestroyNotify(XDestroyWindowEvent *pdwe) {
        XSetCloseDownMode(m_disp, DestroyAll);
        m_quit = true;
    }
};

// the main function
int main(int argc, char **argv) {
    int ret;

    try {
        // X11 multithread
        XInitThreads();

        X11App app(argc, argv);
        if (app.startup()) {
            ret = app.run();
        } else {
            ret = 1;
        }
    } catch (const std::bad_alloc&) {
        fprintf(stderr, PROGNAME ": ERROR: Out of memory\n");
        ret = -1;
    }

    return ret;
}
