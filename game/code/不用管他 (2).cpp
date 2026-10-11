// ============================================================
//  2D Minecraft - 双人分屏版
//  玩家1: WASD + 空格 + 鼠标左键挖/右键放
//  玩家2: 方向键 + Shift + J挖 / K放
// ============================================================

#include <graphics.h>
#include <conio.h>
#include <cstdio>
#include <windows.h>
#include <shellapi.h>
#include <bits/stdc++.h>
#include <cstdarg>
#pragma comment(lib, "shell32.lib")

using namespace std;

// ============================== 日志系统 ==============================
static FILE* gLogFile = nullptr;

void logOpen(const char* path = "debug.log") {
    if (gLogFile) fclose(gLogFile);
    gLogFile = fopen(path, "w");
}

void logClose() {
    if (gLogFile) { fclose(gLogFile); gLogFile = nullptr; }
}

void logWrite(const char* fmt, ...) {
    if (!gLogFile) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(gLogFile, "[%02d:%02d:%02d.%03d] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list a; va_start(a, fmt);
    vfprintf(gLogFile, fmt, a);
    va_end(a);
    fprintf(gLogFile, "\n");
    fflush(gLogFile);
}

// ============================== 常量 ==============================
const int WIN_W = 800;
const int WIN_H = 600;
const int HALF_W = 400;          // 分屏左半边宽度
const int TILE_SIZE = 40;
const int MAP_W = 20000;
const int MAP_H = 200;
const int PLAYER_W = 40;
const int PLAYER_H = 72;

const int WIN_MENU_W = 800;
const int WIN_MENU_H = 600;
const int CREATE_WIN_W = 800;
const int CREATE_WIN_H = 600;

// ============================== 贴图 ==============================
IMAGE grass, stone1, stone2, old_wood, boundary, Player;

// ============================== 玩家结构 ==============================
struct player
{
    int x;
    int y;
    int vx;
    int vy;
    bool onGround;
    bool flying;
};

// 两个玩家
player gP1;
player gP2;

// ============================== 世界 ==============================
int world[200][20000] = {0};

struct WorldSetting
{
    string worldName;
    string worldSeed;
    int gameMode;
    bool allowCheats;
    int worldType;
};

// ============================== UI 控件 ==============================
struct Button
{
    int x, y, w, h;
    string text;
    bool hovered;
    bool pressed;
};

enum CtrlType { CTRL_BUTTON, CTRL_TEXTBOX, CTRL_TOGGLE, CTRL_SELECT };

struct Control
{
    CtrlType type;
    int x, y, w, h;
    string label;
    string placeholder;
    bool hovered;
    bool pressed;
    bool focused;
    bool value;
    int  selectIndex;
    vector<string> options;
    bool expanded;

    Control(): type(CTRL_BUTTON), x(0), y(0), w(0), h(0),
        hovered(false), pressed(false), focused(false),
        value(false), selectIndex(0), expanded(false) {}

    Control(CtrlType t, int x_, int y_, int w_, int h_,
            const string& label_, const string& placeholder_,
            bool hovered_, bool pressed_, bool focused_, bool value_,
            int selectIndex_, const vector<string>& options_)
        : type(t), x(x_), y(y_), w(w_), h(h_),
          label(label_), placeholder(placeholder_),
          hovered(hovered_), pressed(pressed_), focused(focused_),
          value(value_), selectIndex(selectIndex_), options(options_),
          expanded(false) {}
};

vector<Control> controls;
int focusedIndex = -1;

// ============================== 绘图工具 ==============================
void drawPanel(int x, int y, int w, int h, COLORREF fill, COLORREF border)
{
    setfillcolor(border);
    solidrectangle(x, y, x + w, y + h);
    setfillcolor(fill);
    solidrectangle(x + 2, y + 2, x + w - 2, y + h - 2);
}

void setFont(int size, bool bold = false, const char* face = "微软雅黑")
{
    LOGFONT f;
    gettextstyle(&f);
    f.lfHeight = size;
    f.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    strncpy_s(f.lfFaceName, face, LF_FACESIZE - 1);
    f.lfFaceName[LF_FACESIZE - 1] = '\0';
    settextstyle(&f);
}

void outTextCenter(int cx, int cy, const string& s)
{
    int tw = textwidth(s.c_str());
    int th = textheight(s.c_str());
    outtextxy(cx - tw / 2, cy - th / 2, s.c_str());
}

bool inRect(int mx, int my, int x, int y, int w, int h)
{
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

// ============================== 控件绘制 ==============================
void drawButton(Control& c)
{
    COLORREF fill, border, text;
    if (c.pressed)       { fill = RGB(90,90,90);  border = RGB(30,30,30);  text = RGB(200,200,200); }
    else if (c.hovered)  { fill = RGB(150,180,220); border = RGB(200,230,255); text = RGB(255,255,255); }
    else                 { fill = RGB(120,120,120); border = RGB(60,60,60);  text = RGB(230,230,230); }
    int y = c.y + (c.pressed ? 3 : 0);
    drawPanel(c.x, y, c.w, c.h, fill, border);
    setfillcolor(RGB(min(GetRValue(fill)+40,255), min(GetGValue(fill)+40,255), min(GetBValue(fill)+40,255)));
    solidrectangle(c.x + 2, y + 2, c.x + c.w - 2, y + 7);
    setbkmode(TRANSPARENT);
    settextcolor(text);
    setFont(24, true);
    outTextCenter(c.x + c.w / 2, y + c.h / 2, c.label);
}

void drawTextBox(Control& c)
{
    COLORREF border = c.focused ? RGB(255,255,255) : RGB(60,60,60);
    drawPanel(c.x, c.y, c.w, c.h, RGB(30,30,30), border);
    setbkmode(TRANSPARENT);
    string show = c.label.empty() ? c.placeholder : c.label;
    COLORREF tc = c.label.empty() ? RGB(120,120,120) : RGB(230,230,230);
    settextcolor(tc);
    setFont(22, false);
    int ty = c.y + c.h / 2 - textheight(show.c_str()) / 2;
    outtextxy(c.x + 10, ty, show.c_str());
    if (c.focused && (GetTickCount() / 500) % 2 == 0)
    {
        int cursorX = c.x + 12 + textwidth(c.label.c_str());
        setlinecolor(RGB(255,255,255));
        line(cursorX, c.y + 8, cursorX, c.y + c.h - 8);
    }
}

void drawToggle(Control& c)
{
    COLORREF border = c.focused ? RGB(255,255,255) : RGB(60,60,60);
    drawPanel(c.x, c.y, c.w, c.h, RGB(30,30,30), border);
    setbkmode(TRANSPARENT);
    settextcolor(RGB(230,230,230));
    setFont(22, false);
    string show = c.value ? "开启" : "关闭";
    int ty = c.y + c.h / 2 - textheight(show.c_str()) / 2;
    outtextxy(c.x + 10, ty, show.c_str());
    int boxSize = 26;
    int bx = c.x + c.w - boxSize - 8;
    int by = c.y + (c.h - boxSize) / 2;
    setfillcolor(RGB(60,60,60));
    solidrectangle(bx, by, bx + boxSize, by + boxSize);
    if (c.value)
    {
        setlinecolor(RGB(80,220,80));
        setlinestyle(PS_SOLID, 4);
        line(bx + 5, by + 13, bx + 10, by + 19);
        line(bx + 10, by + 19, bx + 21, by + 7);
        setlinestyle(PS_SOLID, 1);
    }
}

void drawSelect(Control& c)
{
    COLORREF border = c.hovered ? RGB(200,230,255) : RGB(60,60,60);
    COLORREF fill   = c.hovered ? RGB(140,170,210) : RGB(120,120,120);
    drawPanel(c.x, c.y, c.w, c.h, fill, border);

    setbkmode(TRANSPARENT);
    settextcolor(RGB(255,255,255));
    setFont(22, false);
    string show = c.options.empty() ? "" : c.options[c.selectIndex];
    int ty = c.y + c.h / 2 - textheight(show.c_str()) / 2;
    outtextxy(c.x + 10, ty, show.c_str());
    setfillcolor(RGB(60,60,60));
    int ax = c.x + c.w - 30;
    int ay = c.y + c.h / 2;
    if (c.expanded)
    {
        POINT pts[3] = {{ax, ay + 6}, {ax + 16, ay + 6}, {ax + 8, ay - 6}};
        solidpolygon(pts, 3);
    }
    else
    {
        POINT pts[3] = {{ax, ay - 6}, {ax + 16, ay - 6}, {ax + 8, ay + 6}};
        solidpolygon(pts, 3);
    }
    if (c.expanded && !c.options.empty())
    {
        int itemH = c.h;
        int listY = c.y + c.h;
        for (int i = 0; i < (int)c.options.size(); i++)
        {
            int iy = listY + i * itemH;
            setfillcolor(i == c.selectIndex ? RGB(80,120,180) : RGB(60,60,60));
            solidrectangle(c.x, iy, c.x + c.w, iy + itemH);
            setlinecolor(RGB(30,30,30));
            rectangle(c.x, iy, c.x + c.w, iy + itemH);
            setbkmode(TRANSPARENT);
            settextcolor(RGB(255,255,255));
            setFont(22, false);
            int ity = iy + itemH / 2 - textheight(c.options[i].c_str()) / 2;
            outtextxy(c.x + 10, ity, c.options[i].c_str());
        }
    }
}

void drawControl(Control& c)
{
    switch(c.type)
    {
        case CTRL_BUTTON:  drawButton(c);  break;
        case CTRL_TEXTBOX: drawTextBox(c); break;
        case CTRL_TOGGLE:  drawToggle(c);  break;
        case CTRL_SELECT:  drawSelect(c);  break;
    }
}

// ============================== 创建世界界面 ==============================
void drawCreateBg()
{
    for(int y=0;y<CREATE_WIN_H;y++)
    {
        float t = (float)y / CREATE_WIN_H;
        setlinecolor(RGB((int)(80 + t*60),(int)(140 + t*50),(int)(200 + t*40)));
        line(0,y,CREATE_WIN_W,y);
    }
    setfillcolor(RGB(255,255,255));
    auto cloud = [](int cx,int cy,int s){
        int b = 20*s;
        solidrectangle(cx,cy,cx+b*4,cy+b);
        solidrectangle(cx+b,cy-b,cx+b*3,cy);
        solidrectangle(cx+b*2,cy-b*2,cx+b*4,cy);
    };
    cloud(60,90,1); cloud(560,60,1); cloud(700,140,1);
    setfillcolor(RGB(80,160,60));
    solidrectangle(0,CREATE_WIN_H-60,CREATE_WIN_W,CREATE_WIN_H);
    setfillcolor(RGB(100,190,80));
    solidrectangle(0,CREATE_WIN_H-60,CREATE_WIN_W,CREATE_WIN_H-50);
}

void initCreateControls(WorldSetting& setting)
{
    controls.clear();
    int panelX = 80;
    int panelY = 60;
    int ctrlX  = 300;
    int ctrlW  = 400;
    int rowH   = 40;
    int rowGap = 70;
    int y = panelY + 120;

    controls.push_back(Control(CTRL_TEXTBOX, ctrlX, y, ctrlW, rowH,
        setting.worldName, "输入世界名称", false, false, false, false, 0, {}));
    y += rowGap;

    Control modeSel;
    modeSel.type = CTRL_SELECT;
    modeSel.x = ctrlX; modeSel.y = y; modeSel.w = ctrlW; modeSel.h = rowH;
    modeSel.selectIndex = (setting.gameMode >= 0 && setting.gameMode < 4) ? setting.gameMode : 0;
    modeSel.options = { "生存模式", "创造模式", "冒险模式", "旁观模式" };
    controls.push_back(modeSel);
    y += rowGap;

    controls.push_back(Control(CTRL_TOGGLE, ctrlX, y, ctrlW, rowH,
        "允许作弊", "允许作弊", false, false, false, setting.allowCheats, 0, {}));
    y += rowGap;

    Control typeSel;
    typeSel.type = CTRL_SELECT;
    typeSel.x = ctrlX; typeSel.y = y; typeSel.w = ctrlW; typeSel.h = rowH;
    typeSel.selectIndex = (setting.worldType >= 0 && setting.worldType < 7) ? setting.worldType : 0;
    typeSel.options = { "默认", "超平坦", "大型生物群系", "放大化",
                        "自定义世界类型一", "自定义世界类型二", "空岛" };
    controls.push_back(typeSel);
    y += rowGap;

    controls.push_back(Control(CTRL_TEXTBOX, ctrlX, y, ctrlW, rowH,
        setting.worldSeed, "留空则随机种子", false, false, false, false, 0, {}));
    y += rowGap + 10;

    int btnW = 160, btnH = 46;
    int totalW = btnW * 2 + 30;
    int bx = (CREATE_WIN_W - totalW) / 2;
    controls.push_back(Control(CTRL_BUTTON, bx, y, btnW, btnH,
        "创建世界", "", false, false, false, false, 0, {}));
    controls.push_back(Control(CTRL_BUTTON, bx + btnW + 30, y, btnW, btnH,
        "取消", "", false, false, false, false, 0, {}));
}

void drawCreateLabels()
{
    setbkmode(TRANSPARENT);
    setFont(24, true);
    settextcolor(RGB(50,50,50));
    outtextxy(101, 81, "创建新的世界");
    settextcolor(RGB(255,255,255));
    outtextxy(100, 80, "创建新的世界");

    int labelX = 120;
    int y = 180;
    setFont(22, false);
    auto shadow = [](int x, int yy, const char* s){
        settextcolor(RGB(50,50,50)); outtextxy(x + 1, yy + 1, s);
        settextcolor(RGB(255,255,255)); outtextxy(x, yy, s);
    };
    shadow(labelX, y + 8, "世界名称"); y += 70;
    shadow(labelX, y + 8, "游戏模式"); y += 70;
    shadow(labelX, y + 8, "允许作弊"); y += 70;
    shadow(labelX, y + 8, "世界类型"); y += 70;
    shadow(labelX, y + 8, "世界种子"); y += 70;
}

void eraseLastChar(string& s)
{
    if (s.empty()) return;
    int i = 0, n = (int)s.size();
    int lastStart = 0;
    while (i < n)
    {
        lastStart = i;
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) i += 1;
        else if ((c & 0xE0) == 0xC0 && i + 1 < n) i += 2;
        else if ((c & 0xF0) == 0xE0 && i + 2 < n) i += 3;
        else if ((c & 0xF8) == 0xF0 && i + 3 < n) i += 4;
        else i += 1;
    }
    if (lastStart < n) s.erase(lastStart);
}

void handleKeyboardCreate(WorldSetting& setting)
{
    if (focusedIndex < 0 || focusedIndex >= (int)controls.size()) return;
    Control& c = controls[focusedIndex];

    ExMessage msg;
    while(peekmessage(&msg, EX_CHAR))
    {
        if(msg.message != WM_CHAR) continue;
        TCHAR ch = (TCHAR)msg.ch;
        if(ch == '\b')
        {
            eraseLastChar(c.label);
        }
        else if(ch == '\r' || ch == '\n')
        {
            focusedIndex = -1;
            return;
        }
        else
        {
            unsigned char uc = (unsigned char)ch;
            if (uc >= 32 && uc != 127 && c.label.size() < 60)
            {
                c.label += (char)uc;
            }
        }
        if (focusedIndex == 0) setting.worldName = c.label;
        if (focusedIndex == 4) setting.worldSeed = c.label;
    }
}

int handleClickCreate(int mx, int my, WorldSetting& setting)
{
    for(int i = 0; i < (int)controls.size(); i++)
    {
        Control& c = controls[i];
        if(c.type != CTRL_SELECT || !c.expanded) continue;
        int itemH = c.h;
        int listY = c.y + c.h;
        int listBottom = listY + (int)c.options.size() * itemH;
        if(mx >= c.x && mx <= c.x + c.w && my >= listY && my <= listBottom)
        {
            int idx = (my - listY) / itemH;
            if(idx >= 0 && idx < (int)c.options.size())
            {
                c.selectIndex = idx;
                if(i == 1) setting.gameMode = idx;
                if(i == 3) setting.worldType = idx;
            }
            c.expanded = false;
            return 0;
        }
        if (inRect(mx, my, c.x, c.y, c.w, c.h))
        {
            c.expanded = false;
            return 0;
        }
        c.expanded = false;
    }
    for(int i = 0; i < (int)controls.size(); i++)
    {
        Control& c = controls[i];
        if(!inRect(mx, my, c.x, c.y, c.w, c.h)) continue;
        if(c.type == CTRL_TEXTBOX)
        {
            focusedIndex = i;
            return 0;
        }
        if(c.type == CTRL_TOGGLE)
        {
            c.value = !c.value;
            setting.allowCheats = c.value;
            return 0;
        }
        if(c.type == CTRL_SELECT)
        {
            c.expanded = true;
            return 0;
        }
        if (c.type == CTRL_BUTTON)
        {
            if (c.label == "创建世界") return 1;
            if (c.label == "取消") return 2;
        }
    }
    focusedIndex = -1;
    return 0;
}

bool showCreateWorldScreen(WorldSetting& outSetting)
{
    initCreateControls(outSetting);
    focusedIndex = -1;
    bool lastClick = false;
    int result = 0;
    while(true)
    {
        if(GetAsyncKeyState(VK_ESCAPE)&0x8000){ result=0; break; }
        POINT pt; GetCursorPos(&pt);
        ScreenToClient(GetHWnd(), &pt);
        int mx=pt.x, my=pt.y;
        bool mouseDown = (GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
        for(auto& c:controls)
        {
            c.hovered = inRect(mx,my,c.x,c.y,c.w,c.h);
            c.pressed = c.hovered && mouseDown && c.type == CTRL_BUTTON;
        }
        if(!mouseDown && lastClick)
        {
            int r = handleClickCreate(mx,my,outSetting);
            if(r ==1) { result=1; break; }
            if(r ==2) { result=0; break; }
        }
        lastClick = mouseDown;
        handleKeyboardCreate(outSetting);
        cleardevice();
        drawCreateBg();
        drawPanel(80, 60, CREATE_WIN_W - 160, CREATE_WIN_H - 160, RGB(100, 100, 100), RGB(50, 50, 50));
        drawCreateLabels();
        for(auto& c : controls)
        {
            if (c.type == CTRL_SELECT && c.expanded) continue;
            drawControl(c);
        }
        for(auto& c : controls)
        {
            if (c.type == CTRL_SELECT && c.expanded) drawSelect(c);
        }
        setFont(18,false);
        settextcolor(RGB(220,220,220));
        outtextxy(20,CREATE_WIN_H-30,"提示：按 ESC 取消");
        FlushBatchDraw();
        Sleep(16);
    }
    return result == 1;
}

// ============================== 世界选择界面 ==============================
unsigned int showWorldSelect()
{
    string dir = "..\\map\\send\\";
    vector<unsigned int> seeds;
    vector<string> names;
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA((dir + "*.txt").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            string name = fd.cFileName;
            if (name.size() > 4)
            {
                string numStr = name.substr(0, name.size() - 4);
                seeds.push_back((unsigned int)strtoul(numStr.c_str(), NULL, 10));
                names.push_back("");
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    initgraph(WIN_MENU_W, WIN_MENU_H);
    setbkcolor(RGB(100, 160, 220));
    cleardevice();
    int selected = -1;
    int scroll = 0;
    bool lastClick = false;
    while (true)
    {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) break;
        ExMessage m;
        while (peekmessage(&m, EX_MOUSE))
        {
            if (m.message == WM_MOUSEWHEEL) scroll -= m.wheel / 120;
        }
        int maxScroll = (int)seeds.size() - 10;
        if (maxScroll < 0) maxScroll = 0;
        if (scroll < 0) scroll = 0;
        if (scroll > maxScroll) scroll = maxScroll;
        POINT pt; GetCursorPos(&pt);
        ScreenToClient(GetHWnd(), &pt);
        int mx = pt.x, my = pt.y;
        bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (!mouseDown && lastClick)
        {
            if (inRect(mx, my, 300, 500, 200, 46))
            {
                selected = -2;
            }
            if (selected == -1)
            {
                for (int i = 0; i < 10; i++)
                {
                    int idx = scroll + i;
                    if (idx >= (int)seeds.size()) break;
                    int rowY = 100 + i * 36;
                    if (inRect(mx, my, 150, rowY, 500, 32))
                    {
                        selected = idx;
                        break;
                    }
                }
            }
        }
        lastClick = mouseDown;
        cleardevice();
        // 背景
        for (int y = 0; y < WIN_MENU_H; y++)
        {
            float t = (float)y / WIN_MENU_H;
            setlinecolor(RGB((int)(80 + t*100), (int)(140 + t*90), (int)(200 + t*55)));
            line(0, y, WIN_MENU_W, y);
        }
        setfillcolor(RGB(80, 160, 60));
        solidrectangle(0, WIN_MENU_H - 80, WIN_MENU_W, WIN_MENU_H);
        setfillcolor(RGB(100, 190, 80));
        solidrectangle(0, WIN_MENU_H - 80, WIN_MENU_W, WIN_MENU_H - 70);

        setbkmode(TRANSPARENT);
        settextcolor(RGB(255, 255, 255));
        setFont(28, true);
        outTextCenter(400, 50, "选择世界");
        setFont(20, false);
        if (seeds.empty())
        {
            settextcolor(RGB(220, 220, 220));
            outTextCenter(400, 200, "暂无存档");
        }
        else
        {
            for (int i = 0; i < 10; i++)
            {
                int idx = scroll + i;
                if (idx >= (int)seeds.size()) break;
                int rowY = 100 + i * 36;
                bool hovered = inRect(mx, my, 150, rowY, 500, 32);
                setfillcolor(hovered ? RGB(150,180,220) : RGB(100,100,100));
                solidrectangle(150, rowY, 650, rowY + 32);
                setbkmode(TRANSPARENT);
                settextcolor(RGB(255, 255, 255));
                char line[200];
                sprintf(line, "世界 (%u)", seeds[idx]);
                outtextxy(165, rowY + 4, line);
            }
        }
        bool hoverNew = inRect(mx, my, 300, 500, 200, 46);
        setfillcolor(hoverNew ? RGB(150,180,220) : RGB(120,120,120));
        solidrectangle(300, 500, 500, 546);
        setlinecolor(RGB(60,60,60));
        rectangle(300, 500, 500, 546);
        setbkmode(TRANSPARENT);
        settextcolor(RGB(255, 255, 255));
        setFont(22, true);
        outTextCenter(400, 523, "创建新的世界");
        setFont(18, false);
        settextcolor(RGB(220, 220, 220));
        outtextxy(20, WIN_MENU_H - 30, "提示：按 ESC 返回");
        FlushBatchDraw();
        Sleep(16);
        if (selected != -1) break;
    }
    if (selected == -1) return 0xFFFFFFFF;
    if (selected == -2) return 0xFFFFFFFE;
    return seeds[selected];
}

// ============================== 主菜单 ==============================
vector<Button> buttons;

void drawBackground()
{
    for (int y = 0; y < WIN_MENU_H; y++)
    {
        float t = (float)y / WIN_MENU_H;
        int r = (int)(80 + t * 100);
        int g = (int)(140 + t * 90);
        int b = (int)(200 + t * 55);
        setlinecolor(RGB(r, g, b));
        line(0, y, WIN_MENU_W, y);
    }
    setfillcolor(RGB(255, 255, 255));
    setlinecolor(RGB(255, 255, 255));
    auto drawCloud = [](int cx, int cy, int scale) {
        int block = 20 * scale;
        solidrectangle(cx,           cy,           cx + block * 4, cy + block);
        solidrectangle(cx + block,   cy - block,   cx + block * 3, cy);
        solidrectangle(cx + block*2, cy - block*2, cx + block * 4, cy);
    };
    drawCloud(80,  80,  1);
    drawCloud(500, 50,  1);
    drawCloud(700, 150, 1);
    drawCloud(250, 160, 1);
    setfillcolor(RGB(80, 160, 60));
    solidrectangle(0, WIN_MENU_H - 80, WIN_MENU_W, WIN_MENU_H);
    setfillcolor(RGB(100, 190, 80));
    solidrectangle(0, WIN_MENU_H - 80, WIN_MENU_W, WIN_MENU_H - 70);
}

void drawTitle()
{
    const char* title = "MINECRAFT";
    settextcolor(RGB(40, 40, 40));
    LOGFONT f;
    gettextstyle(&f);
    f.lfHeight = 90;
    f.lfWeight = FW_HEAVY;
    strncpy_s(f.lfFaceName, "Impact", LF_FACESIZE - 1);
    f.lfFaceName[LF_FACESIZE - 1] = '\0';
    settextstyle(&f);
    int tw = textwidth(title);
    int th = textheight(title);
    int tx = (WIN_MENU_W - tw) / 2;
    int ty = 60;
    setbkmode(TRANSPARENT);
    outtextxy(tx + 5, ty + 5, title);
    settextcolor(RGB(180, 220, 90));
    outtextxy(tx, ty, title);
    settextcolor(RGB(220, 255, 130));
    outtextxy(tx, ty - 2, title);
    settextcolor(RGB(255, 255, 255));
    f.lfHeight = 22;
    f.lfWeight = FW_NORMAL;
    strncpy_s(f.lfFaceName, "微软雅黑", LF_FACESIZE - 1);
    f.lfFaceName[LF_FACESIZE - 1] = '\0';
    settextstyle(&f);
    const char* sub = "Java Edition";
    int sw = textwidth(sub);
    outtextxy((WIN_MENU_W - sw) / 2, ty + th + 10, sub);
}

void drawVersion()
{
    settextcolor(RGB(255, 255, 255));
    setFont(18, false, "Consolas");
    setbkmode(TRANSPARENT);
    outtextxy(10, WIN_MENU_H - 30, "2D Minecraft 双人版  v2.0");
    outtextxy(WIN_MENU_W - 220, WIN_MENU_H - 30, "Copyright Mojang AB");
}

void drawMinecraftButton(Button& btn)
{
    int x = btn.x, y = btn.y, w = btn.w, h = btn.h;
    COLORREF topColor, borderColor, textColor;
    if (btn.pressed)
    {
        topColor    = RGB(90, 90, 90);
        borderColor = RGB(30, 30, 30);
        textColor   = RGB(200, 200, 200);
        y += 3;
    }
    else if (btn.hovered)
    {
        topColor = RGB(150, 180, 220);
        borderColor = RGB(200, 230, 255);
        textColor = RGB(255, 255, 255);
    }
    else
    {
        topColor = RGB(120, 120, 120);
        borderColor = RGB(60, 60, 60);
        textColor = RGB(230, 230, 230);
    }
    setfillcolor(borderColor);
    solidrectangle(x, y, x + w, y + h);
    setfillcolor(topColor);
    solidrectangle(x + 3, y + 3, x + w - 3, y + h - 3);
    setfillcolor(RGB(min(GetRValue(topColor) + 40, 255),
                     min(GetGValue(topColor) + 40, 255),
                     min(GetBValue(topColor) + 40, 255)));
    solidrectangle(x + 3, y + 3, x + w - 3, y + 8);
    setfillcolor(RGB(GetRValue(borderColor), GetGValue(borderColor), GetBValue(borderColor)));
    solidrectangle(x + 3, y + h - 8, x + w - 3, y + h - 3);
    setbkmode(TRANSPARENT);
    settextcolor(textColor);
    setFont(28, true);
    int tx = x + w / 2 - textwidth(btn.text.c_str()) / 2;
    int ty = y + h / 2 - textheight(btn.text.c_str()) / 2;
    outtextxy(tx, ty, btn.text.c_str());
}

bool isInButton(Button& btn, int mx, int my)
{
    return mx >= btn.x && mx <= btn.x + btn.w && my >= btn.y && my <= btn.y + btn.h;
}

// ============================== 地图生成 ==============================
void loadMap(unsigned int seed1);

unsigned int seed;
int gameMode = 0;
int savedPlayerX = 100;
int savedPlayerY = 0;
bool hasSavedPos = false;

void savePlayerPos(unsigned int seed1, int px, int py)
{
    string path = "..\\map\\send\\" + to_string(seed1) + ".txt";
    vector<string> header;
    ifstream fin(path.c_str());
    string line;
    while (getline(fin, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "---MAP---") break;
        header.push_back(line);
    }
    fin.close();
    bool foundX = false, foundY = false;
    for (auto& l : header)
    {
        if (l.size() >= 8 && l.substr(0, 8) == "playerX=") { l = "playerX=" + to_string(px); foundX = true; }
        if (l.size() >= 8 && l.substr(0, 8) == "playerY=") { l = "playerY=" + to_string(py); foundY = true; }
    }
    if (!foundX) header.push_back("playerX=" + to_string(px));
    if (!foundY) header.push_back("playerY=" + to_string(py));
    ofstream fout(path.c_str());
    for (auto& l : header) fout << l << "\n";
    fout << "---MAP---\n";
    for (int y = 0; y < MAP_H; y++)
    {
        for (int x = 0; x < MAP_W; x++)
        {
            fout << world[y][x];
            if (x != MAP_W - 1) fout << ",";
        }
        fout << "\n";
    }
    fout.close();
}

void generateMapBySetting(const WorldSetting& setting)
{
    memset(world, 0, sizeof(world));
    if (!setting.worldSeed.empty())
    {
        seed = 0;
        for (auto ch : setting.worldSeed) seed = seed * 1113 + ch;
    }
    else
    {
        seed = (unsigned int)(GetTickCount() ^ (time(nullptr) << 16));
    }
    string dir = "..\\map\\send\\";
    string fileName = dir + to_string(seed) + ".txt";

    ifstream test(fileName.c_str());
    if (test.good())
    {
        test.close();
        loadMap(seed);
        return;
    }
    srand(seed);

    if (setting.worldType == 1)
    {
        for (int x = 0; x < MAP_W; x++)
        {
            world[MAP_H - 1][x] = 5;
            world[MAP_H - 2][x] = 2;
            world[MAP_H - 3][x] = 2;
            world[MAP_H - 4][x] = 1;
        }
    }
    else
    {
        int curY = MAP_H - 4;
        for (int x = 0; x < MAP_W; x++)
        {
            int delta = (rand() % 3) - 1;
            curY += delta;
            if (curY > MAP_H - 4) curY = MAP_H - 4;
            if (curY < MAP_H - 8) curY = MAP_H - 8;

            world[curY][x] = 1;
            for (int y = curY + 1; y < MAP_H - 1; y++)
            {
                world[y][x] = 2;
            }
            world[MAP_H - 1][x] = 5;
        }
    }
    ofstream fout(fileName.c_str());
    if (!fout.is_open())
    {
        CreateDirectoryA(dir.c_str(), NULL);
        fout.open(fileName.c_str());
    }
    if (fout.is_open())
    {
        fout << "worldName=" << setting.worldName   << "\n";
        fout << "worldSeed=" << setting.worldSeed   << "\n";
        fout << "gameMode=" << setting.gameMode    << "\n";
        fout << "allowCheats=" << setting.allowCheats << "\n";
        fout << "worldType=" << setting.worldType   << "\n";
        fout << "randSeed=" << seed                << "\n";
        fout << "playerX=100\n";
        fout << "playerY=0\n";
        fout << "---MAP---\n";
        for (int y = 0; y < MAP_H; y++)
        {
            for (int x = 0; x < MAP_W; x++)
            {
                fout << world[y][x];
                if (x != MAP_W - 1) fout << ",";
            }
            fout << "\n";
        }
        fout.close();
    }
}

// ============================== 主菜单初始化 ==============================
unsigned int interfaceInit()
{
    initgraph(WIN_MENU_W, WIN_MENU_H);
    setbkcolor(RGB(100, 160, 220));
    cleardevice();
    int btnW = 400, btnH = 50;
    int startX = (WIN_MENU_W - btnW) / 2;
    int startY = 280;
    int gap = 20;
    buttons.clear();
    buttons.push_back({ startX, startY + (btnH + gap) * 0, btnW, btnH, "单人游戏", false, false });
    buttons.push_back({ startX, startY + (btnH + gap) * 1, btnW, btnH, "多人游戏", false, false });
    buttons.push_back({ startX, startY + (btnH + gap) * 2, btnW, btnH, "下载新版本", false, false });
    buttons.push_back({ startX, startY + (btnH + gap) * 3, btnW, btnH, "退出游戏", false, false });
    bool lastClick = false;
    int  clickedIndex = -1;
    BeginBatchDraw();
    while (true)
    {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) break;
        peekmessage(nullptr, EX_KEY | EX_MOUSE);
        POINT pt;
        GetCursorPos(&pt);
        HWND hwnd = GetHWnd();
        ScreenToClient(hwnd, &pt);
        int mx = pt.x;
        int my = pt.y;
        bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        for (auto& btn : buttons)
        {
            btn.hovered = isInButton(btn, mx, my);
            btn.pressed = btn.hovered && mouseDown;
        }
        if (!mouseDown && lastClick)
        {
            for (int i = 0; i < (int)buttons.size(); i++)
            {
                if (isInButton(buttons[i], mx, my))
                {
                    clickedIndex = i;
                    break;
                }
            }
        }
        lastClick = mouseDown;
        cleardevice();
        drawBackground();
        drawTitle();
        for (auto& btn : buttons) drawMinecraftButton(btn);
        drawVersion();
        if (clickedIndex != -1)
        {
            switch (clickedIndex)
            {
                case 0:
                {
                    EndBatchDraw();
                    unsigned int s = showWorldSelect();
                    if (s == 0xFFFFFFFF)
                    {
                        initgraph(WIN_MENU_W, WIN_MENU_H);
                        setbkcolor(RGB(100, 160, 220));
                        cleardevice();
                        BeginBatchDraw();
                        break;
                    }
                    if (s == 0xFFFFFFFE)
                    {
                        WorldSetting ws;
                        ws.worldName = "新的世界";
                        ws.worldSeed = "";
                        ws.gameMode = 0;
                        ws.allowCheats = false;
                        ws.worldType = 0;
                        EndBatchDraw();
                        bool ok = showCreateWorldScreen(ws);
                        if (ok)
                        {
                            generateMapBySetting(ws);
                            closegraph();
                            return seed;
                        }
                        initgraph(WIN_MENU_W, WIN_MENU_H);
                        setbkcolor(RGB(100, 160, 220));
                        cleardevice();
                        BeginBatchDraw();
                        break;
                    }
                    closegraph();
                    return s;
                }
                case 1:
                    MessageBoxA(GetHWnd(), "多人游戏：请用双人模式进入单人游戏（分屏）", "Minecraft", MB_OK | MB_ICONINFORMATION);
                    break;
                case 2:
                    ShellExecuteA(NULL, "open", "index.html", NULL, NULL, SW_SHOWNORMAL);
                    break;
                case 3:
                    EndBatchDraw();
                    closegraph();
                    return 0xFFFFFFFF;
            }
            clickedIndex = -1;
        }
        FlushBatchDraw();
        Sleep(16);
    }
    EndBatchDraw();
    closegraph();
    return 0xFFFFFFFF;
}

// ============================== 游戏工具 ==============================
bool isSolid(int gx, int gy)
{
    if (gx < 0 || gx >= MAP_W || gy < 0 || gy >= MAP_H) return false;
    return world[gy][gx] != 0;
}

void loadMap(unsigned int seed1)
{
    string path = "..\\map\\send\\" + to_string(seed1) + ".txt";
    ifstream file(path.c_str());
    if (!file.is_open()) return;
    hasSavedPos = false;
    savedPlayerX = 100;
    savedPlayerY = 0;
    string line;
    while (getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "---MAP---") break;
        if (line.size() >= 8 && line.substr(0, 8) == "playerX=")
        {
            savedPlayerX = atoi(line.substr(8).c_str());
            hasSavedPos = true;
        }
        else if (line.size() >= 8 && line.substr(0, 8) == "playerY=")
        {
            savedPlayerY = atoi(line.substr(8).c_str());
            hasSavedPos = true;
        }
        else if (line.size() >= 9 && line.substr(0, 9) == "gameMode=")
        {
            gameMode = atoi(line.substr(9).c_str());
        }
    }
    for (int y = 0; y < MAP_H; y++)
    {
        if (!getline(file, line)) break;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        stringstream ss(line);
        string cell;
        int x = 0;
        while (getline(ss, cell, ',') && x < MAP_W) world[y][x++] = atoi(cell.c_str());
    }
    file.close();
}

// ============================== 只画可见区域 ==============================
void drawMap(int camX, int camY, int viewW, int viewH, int offsetX)
{
    int x0 = camX / TILE_SIZE;              if (x0 < 0) x0 = 0;
    int y0 = camY / TILE_SIZE;              if (y0 < 0) y0 = 0;
    int x1 = (camX + viewW) / TILE_SIZE + 1; if (x1 > MAP_W) x1 = MAP_W;
    int y1 = (camY + viewH) / TILE_SIZE + 1; if (y1 > MAP_H) y1 = MAP_H;

    for (int y = y0; y < y1; y++)
    {
        for (int x = x0; x < x1; x++)
        {
            int px = x * TILE_SIZE - camX + offsetX;
            int py = y * TILE_SIZE - camY;
            if (px < offsetX - TILE_SIZE || px > offsetX + viewW) continue;
            if (py < -TILE_SIZE || py > viewH) continue;
            if (world[y][x] == 1) putimage(px, py, &grass);
            else if (world[y][x] == 2) putimage(px, py, &stone1);
            else if (world[y][x] == 3) putimage(px, py, &old_wood);
            else if (world[y][x] == 4) putimage(px, py, &stone2);
            else if (world[y][x] == 5) putimage(px, py, &boundary);
        }
    }
}

// ============================== 玩家2 的物理更新 ==============================
void updatePlayer2()
{
    gP2.vx = 0;
    if (GetAsyncKeyState(VK_LEFT)  & 0x8000) gP2.vx = -5;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) gP2.vx =  5;

    bool p2Jump = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (p2Jump && gP2.onGround) { gP2.vy = -12; gP2.onGround = false; }

    // 水平
    int newX = gP2.x + gP2.vx;
    int top = gP2.y;
    int bot = gP2.y + 72;
    bool hit = false;
    if (gP2.vx > 0)
    {
        int cgx = (newX + 39) / 40;
        for (int gy = top / 40; gy <= bot / 40; gy++)
            if (isSolid(cgx, gy)) { hit = true; break; }
        if (hit) gP2.x = cgx * 40 - 40;
        else gP2.x = newX;
    }
    else if (gP2.vx < 0)
    {
        int cgx = newX / 40;
        for (int gy = top / 40; gy <= bot / 40; gy++)
            if (isSolid(cgx, gy)) { hit = true; break; }
        if (hit) gP2.x = cgx * 40 + 40;
        else gP2.x = newX;
    }
    else gP2.x = newX;
    if (gP2.x < 0) gP2.x = 0;
    if (gP2.x + 40 > MAP_W * 40) gP2.x = MAP_W * 40 - 40;

    // 垂直
    gP2.vy += 1;
    int newY = gP2.y + gP2.vy;
    gP2.onGround = false;
    int gx1 = gP2.x / 40;
    int gx2 = (gP2.x + 39) / 40;
    if (gP2.vy > 0)
    {
        int footY = (newY + 72) / 40;
        if (isSolid(gx1, footY) || isSolid(gx2, footY))
        {
            gP2.y = footY * 40 - 73;
            gP2.vy = 0;
            gP2.onGround = true;
        }
        else gP2.y = newY;
    }
    else if (gP2.vy < 0)
    {
        int headY = newY / 40;
        if (isSolid(gx1, headY) || isSolid(gx2, headY))
        {
            gP2.y = headY * 40 + 40;
            gP2.vy = 0;
        }
        else gP2.y = newY;
    }
    else gP2.y = newY;
}

// ============================== 主游戏循环 ==============================
void runGame(unsigned int seed1)
{
    logWrite("runGame: 开始, seed=%u", seed1);

    srand((unsigned)time(NULL));
    logWrite("runGame: 初始化图形窗口");
    initgraph(WIN_W, WIN_H);
    logWrite("runGame: 图形窗口 OK");

    logWrite("runGame: 加载贴图");
    loadimage(&grass,    "..\\block\\grass.png");
    loadimage(&stone1,   "..\\block\\stone1.png");
    loadimage(&stone2,   "..\\block\\stone2.png");
    loadimage(&boundary, "..\\block\\boundary.png");
    loadimage(&old_wood, "..\\block\\old_wood.png");
    loadimage(&Player,   "..\\player\\player.png");
    logWrite("runGame: 贴图加载完成");

    logWrite("runGame: 加载地图");
    loadMap(seed1);
    logWrite("runGame: 地图加载完成, player=(%d,%d)", savedPlayerX, savedPlayerY);

    // 初始化玩家1
    gP1 = {savedPlayerX, savedPlayerY, 0, 0, false, false};
    gP1.flying = false;

    // 初始化玩家2（在玩家1右边 120 像素）
    gP2 = {savedPlayerX + 120, savedPlayerY, 0, 0, false, false};
    gP2.flying = false;

    // 找地面
    {
        int gx1 = gP2.x / 40;
        int gx2 = (gP2.x + 39) / 40;
        bool ok = false;
        for (int G = 1; G < MAP_H; G++)
        {
            bool floor = isSolid(gx1, G) || isSolid(gx2, G);
            bool s1 = !isSolid(gx1, G-1) && !isSolid(gx2, G-1);
            bool s2 = (G-2<0) ? true : (!isSolid(gx1, G-2) && !isSolid(gx2, G-2));
            if (floor && s1 && s2) { gP2.y = G*40 - 73; gP2.onGround = true; ok = true; break; }
        }
        if (!ok) gP2.y = gP1.y;
    }

    // 玩家1 出生点检测
    if (!hasSavedPos)
    {
        if (gameMode == 1)
        {
            gP1.y = 200;
            gP1.onGround = false;
        }
        else
        {
            int gx1 = gP1.x / 40;
            int gx2 = (gP1.x + 39) / 40;
            bool spawned = false;
            for (int G = 1; G < MAP_H; G++)
            {
                bool hasFloor = isSolid(gx1, G) || isSolid(gx2, G);
                bool space1 = !isSolid(gx1, G - 1) && !isSolid(gx2, G - 1);
                bool space2 = (G - 2 < 0) ? true : (!isSolid(gx1, G - 2) && !isSolid(gx2, G - 2));
                if (hasFloor && space1 && space2)
                {
                    gP1.y = G * 40 - 73;
                    gP1.vy = 0;
                    gP1.onGround = true;
                    spawned = true;
                    break;
                }
            }
            if (!spawned) gP1.y = MAP_H * 40 - 73;
        }
    }

    // 两个摄像机
    int cam1X = 0, cam1Y = 0;
    int cam2X = 0, cam2Y = 0;

    DWORD fpsTime = GetTickCount();
    int fpsCount = 0;
    int fpsValue = 0;

    bool showDebug = false;
    bool f3Prev = false;
    bool paused = false;
    bool escPrev = false;
    bool pauseClickPrev = false;

    bool lClickPrev = false;
    bool rClickPrev = false;
    bool kPrev = false;

    IMAGE pauseBg(WIN_W, WIN_H);
    bool pauseBgReady = false;

    logWrite("runGame: 进入主循环");

    while(true)
    {
        // ==================== ESC 暂停 ====================
        bool escNow = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        if (escNow && !escPrev)
        {
            paused = !paused;
            if (paused)
            {
                FlushBatchDraw();
                getimage(&pauseBg, 0, 0, WIN_W, WIN_H);
                pauseBgReady = true;
            }
        }
        escPrev = escNow;

        if (paused)
        {
            POINT pt; GetCursorPos(&pt);
            ScreenToClient(GetHWnd(), &pt);
            int mx = pt.x, my = pt.y;
            bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

            BeginBatchDraw();
            cleardevice();
            if (pauseBgReady) putimage(0, 0, &pauseBg);

            setbkmode(TRANSPARENT);
            settextcolor(RGB(255, 255, 255));
            setFont(48, true);
            outTextCenter(400, 120, "游戏暂停");

            int btnX = 300, btnW = 200, btnH = 50;
            int b1 = 250, b2 = 320, b3 = 390;

            auto drawPauseBtn = [&](int y, const char* txt) {
                bool hovered = inRect(mx, my, btnX, y, btnW, btnH);
                setfillcolor(hovered ? RGB(150,180,220) : RGB(120,120,120));
                solidrectangle(btnX, y, btnX + btnW, y + btnH);
                setlinecolor(RGB(60,60,60));
                rectangle(btnX, y, btnX + btnW, y + btnH);
                setbkmode(TRANSPARENT);
                settextcolor(RGB(255, 255, 255));
                setFont(24, true);
                outTextCenter(btnX + btnW / 2, y + btnH / 2, txt);
            };
            drawPauseBtn(b1, "继续游戏");
            drawPauseBtn(b2, "说明");
            drawPauseBtn(b3, "保存并退出");

            if (!mouseDown && pauseClickPrev)
            {
                if (inRect(mx, my, btnX, b1, btnW, btnH))
                {
                    paused = false;
                }
                else if (inRect(mx, my, btnX, b2, btnW, btnH))
                {
                    MessageBoxA(GetHWnd(),
                        "玩家1: WASD 移动, 空格跳, 鼠标左右键挖/放\n"
                        "玩家2: 方向键移动, Shift跳, J挖, K放",
                        "操作说明", MB_OK);
                }
                else if (inRect(mx, my, btnX, b3, btnW, btnH))
                {
                    savePlayerPos(seed1, gP1.x, gP1.y);
                    logWrite("保存并退出");
                    EndBatchDraw();
                    closegraph();
                    return;
                }
            }
            pauseClickPrev = mouseDown;
            EndBatchDraw();
            Sleep(16);
            continue;
        }
        pauseClickPrev = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        // ==================== 摄像机 ====================
        cam1X = gP1.x + 20 - HALF_W / 2;
        cam1Y = gP1.y + 36 - WIN_H / 2;
        cam2X = gP2.x + 20 - HALF_W / 2;
        cam2Y = gP2.y + 36 - WIN_H / 2;

        if (cam1X < 0) cam1X = 0;
        if (cam1Y < 0) cam1Y = 0;
        if (cam1X > MAP_W * 40 - HALF_W) cam1X = MAP_W * 40 - HALF_W;
        if (cam1Y > MAP_H * 40 - WIN_H) cam1Y = MAP_H * 40 - WIN_H;

        if (cam2X < 0) cam2X = 0;
        if (cam2Y < 0) cam2Y = 0;
        if (cam2X > MAP_W * 40 - HALF_W) cam2X = MAP_W * 40 - HALF_W;
        if (cam2Y > MAP_H * 40 - WIN_H) cam2Y = MAP_H * 40 - WIN_H;

        // ==================== 鼠标（玩家1） ====================
        POINT mpt;
        GetCursorPos(&mpt);
        ScreenToClient(GetHWnd(), &mpt);
        int mwx = (mpt.x + cam1X) / 40;
        int mwy = (mpt.y + cam1Y) / 40;
        bool lNow = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool rNow = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        int pcx = (gP1.x + 20) / 40;
        int pcy = (gP1.y + 36) / 40;
        int dist = abs(mwx - pcx) + abs(mwy - pcy);

        // 玩家1 挖
        if (lNow && mwx >= 0 && mwx < MAP_W && mwy >= 0 && mwy < MAP_H && dist <= 5)
        {
            if (world[mwy][mwx] != 5) world[mwy][mwx] = 0;
        }
        // 玩家1 放
        if (rNow && !rClickPrev && mwx >= 0 && mwx < MAP_W && mwy >= 0 && mwy < MAP_H && dist <= 5)
        {
            if (world[mwy][mwx] == 0)
            {
                bool hasNb = (mwx>0 && world[mwy][mwx-1]!=0) ||
                             (mwx<MAP_W-1 && world[mwy][mwx+1]!=0) ||
                             (mwy>0 && world[mwy-1][mwx]!=0) ||
                             (mwy<MAP_H-1 && world[mwy+1][mwx]!=0);
                if (hasNb) world[mwy][mwx] = 4;
            }
        }
        lClickPrev = lNow;
        rClickPrev = rNow;

        // ==================== 玩家1 输入 ====================
        gP1.vx = 0;
        if (GetAsyncKeyState('A') & 0x8000) gP1.vx = -5;
        if (GetAsyncKeyState('D') & 0x8000) gP1.vx =  5;
        bool spaceNow = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        if (spaceNow && gP1.onGround) { gP1.vy = -12; gP1.onGround = false; }

        // 玩家1 水平
        {
            int nX = gP1.x + gP1.vx;
            int top = gP1.y;
            int bot = gP1.y + 72;
            bool hit = false;
            if (gP1.vx > 0)
            {
                int cgx = (nX + 39) / 40;
                for (int gy = top/40; gy <= bot/40; gy++)
                    if (isSolid(cgx, gy)) { hit = true; break; }
                if (hit) gP1.x = cgx*40 - 40; else gP1.x = nX;
            }
            else if (gP1.vx < 0)
            {
                int cgx = nX / 40;
                for (int gy = top/40; gy <= bot/40; gy++)
                    if (isSolid(cgx, gy)) { hit = true; break; }
                if (hit) gP1.x = cgx*40 + 40; else gP1.x = nX;
            }
            else gP1.x = nX;
            if (gP1.x < 0) gP1.x = 0;
            if (gP1.x + 40 > MAP_W*40) gP1.x = MAP_W*40 - 40;
        }

        // 玩家1 垂直
        {
            gP1.vy += 1;
            int nY = gP1.y + gP1.vy;
            gP1.onGround = false;
            int gx1 = gP1.x / 40;
            int gx2 = (gP1.x + 39) / 40;
            if (gP1.vy > 0)
            {
                int footY = (nY + 72) / 40;
                if (isSolid(gx1, footY) || isSolid(gx2, footY))
                {
                    gP1.y = footY*40 - 73;
                    gP1.vy = 0;
                    gP1.onGround = true;
                }
                else gP1.y = nY;
            }
            else if (gP1.vy < 0)
            {
                int headY = nY / 40;
                if (isSolid(gx1, headY) || isSolid(gx2, headY))
                {
                    gP1.y = headY*40 + 40;
                    gP1.vy = 0;
                }
                else gP1.y = nY;
            }
            else gP1.y = nY;
        }

        // ==================== 玩家2 输入 ====================
        updatePlayer2();

        // 玩家2 挖（J 键）
        int p2cx = (gP2.x + 20) / 40;
        int p2cy = (gP2.y + 36) / 40;
        int p2tx = p2cx + (gP2.vx >= 0 ? 2 : -2);
        int p2ty = p2cy;

        if (GetAsyncKeyState('J') & 0x8000)
        {
            if (p2tx >= 0 && p2tx < MAP_W && p2ty >= 0 && p2ty < MAP_H)
                if (world[p2ty][p2tx] != 5) world[p2ty][p2tx] = 0;
        }
        // 玩家2 放（K 键）
        bool kNow = (GetAsyncKeyState('K') & 0x8000) != 0;
        if (kNow && !kPrev)
        {
            if (p2tx >= 0 && p2tx < MAP_W && p2ty >= 0 && p2ty < MAP_H)
            {
                if (world[p2ty][p2tx] == 0)
                {
                    bool hasNb = (p2tx>0 && world[p2ty][p2tx-1]!=0) ||
                                 (p2tx<MAP_W-1 && world[p2ty][p2tx+1]!=0) ||
                                 (p2ty>0 && world[p2ty-1][p2tx]!=0) ||
                                 (p2ty<MAP_H-1 && world[p2ty+1][p2tx]!=0);
                    if (hasNb) world[p2ty][p2tx] = 4;
                }
            }
        }
        kPrev = kNow;

        // ==================== FPS ====================
        fpsCount++;
        if (GetTickCount() - fpsTime >= 1000)
        {
            fpsValue = fpsCount;
            fpsCount = 0;
            fpsTime = GetTickCount();
        }

        // ==================== 调试 ====================
        bool f3Now = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
        if (f3Now && !f3Prev) showDebug = !showDebug;
        f3Prev = f3Now;

        // ==================== 渲染 ====================
        BeginBatchDraw();
        cleardevice();

        // ---------- 左半边：玩家1 ----------
        // 用 setcliprgn 裁剪（EasyX 支持）
        setcliprgn(0, 0, HALF_W, WIN_H);
        drawMap(cam1X, cam1Y, HALF_W, WIN_H, 0);
        putimage(gP1.x - cam1X, gP1.y - cam1Y, &Player);
        putimage(gP2.x - cam1X, gP2.y - cam1Y, &Player);
        setcliprgn(0, 0, WIN_W, WIN_H);  // 恢复

        // ---------- 右半边：玩家2 ----------
        setcliprgn(HALF_W, 0, WIN_W, WIN_H);
        drawMap(cam2X, cam2Y, HALF_W, WIN_H, HALF_W);
        putimage(gP1.x - cam2X + HALF_W, gP1.y - cam2Y, &Player);
        putimage(gP2.x - cam2X + HALF_W, gP2.y - cam2Y, &Player);
        setcliprgn(0, 0, WIN_W, WIN_H);

        // ---------- 分屏线 ----------
        setlinecolor(RGB(0, 0, 0));
        setlinestyle(PS_SOLID, 3);
        line(HALF_W, 0, HALF_W, WIN_H);
        setlinestyle(PS_SOLID, 1);

        // ---------- 玩家标签 ----------
        setbkmode(TRANSPARENT);
        setFont(20, true);
        settextcolor(RGB(80, 200, 255));
        outtextxy(10, 10, "玩家1");
        settextcolor(RGB(255, 120, 80));
        outtextxy(HALF_W + 10, 10, "玩家2");

        // ---------- 调试信息 ----------
        if (showDebug)
        {
            char buf[128];
            settextcolor(RGB(255, 255, 0));
            sprintf(buf, "FPS: %d", fpsValue);
            outtextxy(10, 40, buf);

            sprintf(buf, "P1: (%d, %d)", gP1.x / 40, gP1.y / 40);
            outtextxy(10, 65, buf);

            settextcolor(RGB(255, 180, 130));
            sprintf(buf, "P2: (%d, %d)", gP2.x / 40, gP2.y / 40);
            outtextxy(HALF_W + 10, 40, buf);
        }

        EndBatchDraw();
        Sleep(16);
    }
}

// ============================== main ==============================
int main()
{
    logOpen("debug.log");
    logWrite("=== 程序启动 ===");

    while (true)
    {
        logWrite("--- 进入主菜单 ---");
        unsigned int seed1 = interfaceInit();
        logWrite("主菜单返回 seed = %u", seed1);

        if (seed1 == 0xFFFFFFFF)
        {
            logWrite("用户退出游戏");
            break;
        }

        logWrite("--- 开始加载世界 seed=%u ---", seed1);
        runGame(seed1);
        logWrite("--- 世界 %u 已退出 ---", seed1);
    }

    logWrite("=== 程序正常结束 ===");
    logClose();
    return 0;
}