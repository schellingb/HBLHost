//----------------------------------------------------------------//
// HBLHost                                                        //
// License:  GNU GENERAL PUBLIC LICENSE - Version 3, 29 June 2007 //
//----------------------------------------------------------------//

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef _DEBUG
#define HBLHOST_USE_WIN32SYSTRAY
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#define STS_NET_IMPLEMENTATION
#define STS_NET_NO_PACKETS
#define STS_NET_NO_ERRORSTRINGS
#include "sts_net.inl"

static sts_net_interfaceinfo_t g_AddressTable[20];
static int g_AddressCount;
static int g_Port = 5000;
static const char s_ResponseTEXT[] = "HTTP/1.1 200 OK" "\r\n" "\r\n" "HELLO WORLD";
#ifndef HBLHOST_TEXTRESPONSEONLY
static const char s_ResponseMP4[] = "HTTP/1.1 200 OK" "\r\n" "Content-Type: video/mp4" "\r\n" "\r\n";
static const char s_ResponseREDIRECT[] = "HTTP/1.1 303 Moved Permanently" "\r\n" "Location: /MP4" "\r\n" "\r\n";
static bool g_ServeTEXT;
static char* g_MP4ExecuteELF;
static unsigned char mp4[41224];
static void inflate_mp4();
#endif

#ifdef HBLHOST_USE_WIN32SYSTRAY
#include <shellapi.h>

static DWORD CALLBACK SystrayThread(LPVOID) 
{
	struct Wnd
	{
		enum { IDM_NONE, IDM_MP4, IDM_TEXT, IDM_EXIT };

		static void OpenMenu(HWND hWnd, bool AtCursor)
		{
			static POINT lpClickPoint;
			if (AtCursor) GetCursorPos(&lpClickPoint);
			HMENU hPopMenu = CreatePopupMenu();
			InsertMenuA(hPopMenu,0xFFFFFFFF,MF_STRING|MF_GRAYED,IDM_NONE, "HBLHOST");
			InsertMenuA(hPopMenu,0xFFFFFFFF,MF_STRING|MF_GRAYED,IDM_NONE, g_MP4ExecuteELF);
			InsertMenuA(hPopMenu,0xFFFFFFFF,MF_SEPARATOR,IDM_NONE,NULL);
			for (int i = 0; i < g_AddressCount; i++)
			{
				char buf[256];
				sprintf(buf, (g_Port == 80 ? "@ http://%s/" : "@ http://%s:%d/"), g_AddressTable[i].address, g_Port);
				InsertMenuA(hPopMenu,0xFFFFFFFF,MF_STRING|MF_GRAYED,IDM_NONE, buf);
			}
			InsertMenuA(hPopMenu,0xFFFFFFFF,MF_SEPARATOR,IDM_NONE,NULL);
			InsertMenuA(hPopMenu,0xFFFFFFFF,MF_STRING|(g_ServeTEXT ? MF_UNCHECKED : MF_CHECKED),IDM_MP4, "Serve HBL MP4 file");
			InsertMenuA(hPopMenu,0xFFFFFFFF,MF_STRING|(g_ServeTEXT ? MF_CHECKED : MF_UNCHECKED),IDM_TEXT, "Serve plain text page");
			InsertMenuA(hPopMenu,0xFFFFFFFF,MF_SEPARATOR,IDM_NONE,NULL);
			InsertMenuA(hPopMenu,0xFFFFFFFF,MF_STRING,IDM_EXIT,"Exit");
			SetForegroundWindow(hWnd); //cause the popup to be focused
			TrackPopupMenu(hPopMenu,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_BOTTOMALIGN, lpClickPoint.x, lpClickPoint.y,0,hWnd,NULL);
		}

		static LRESULT CALLBACK Proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
		{
			//const LPCSTR GetMsgString(UINT Msg);
			//if (Msg != WM_ENTERIDLE && (Msg != WM_USER || LOWORD(lParam) != WM_MOUSEMOVE))
			//	printf("[MSG] HWND: %x - Msg: %x [%s] - wParam: %x (%x [%s] / %x [%s]) - lParam: %x (%x [%s] / %x [%s])\n", hWnd, Msg, GetMsgString(Msg), wParam, LOWORD(wParam), GetMsgString(LOWORD(wParam)), HIWORD(wParam), GetMsgString(HIWORD(wParam)), lParam, LOWORD(lParam), GetMsgString(LOWORD(lParam)), HIWORD(lParam), GetMsgString(HIWORD(lParam)));
			static UINT s_WM_TASKBARRESTART;
			if (Msg == WM_COMMAND && wParam == IDM_MP4)  { g_ServeTEXT = false; OpenMenu(hWnd, false); }
			if (Msg == WM_COMMAND && wParam == IDM_TEXT) { g_ServeTEXT = true;  OpenMenu(hWnd, false); }
			if (Msg == WM_COMMAND && wParam == IDM_EXIT) { NOTIFYICONDATAA i; ZeroMemory(&i, sizeof(i)); i.cbSize = sizeof(i); i.hWnd = hWnd; Shell_NotifyIconA(NIM_DELETE, &i); ExitProcess(EXIT_SUCCESS); }
			if (Msg == WM_USER && (LOWORD(lParam) == WM_LBUTTONUP || LOWORD(lParam) == WM_RBUTTONUP)) OpenMenu(hWnd, true); //systray rightclick
			if (Msg == WM_CREATE || Msg == s_WM_TASKBARRESTART)
			{
				if (Msg == WM_CREATE) s_WM_TASKBARRESTART = RegisterWindowMessageA("TaskbarCreated");
				NOTIFYICONDATAA nid;
				ZeroMemory(&nid, sizeof(nid));
				nid.cbSize = sizeof(nid); 
				nid.hWnd = hWnd;
				nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; 
				nid.hIcon = LoadIconA(GetModuleHandleA(NULL), "ICN");
				nid.uCallbackMessage = WM_USER; 
				strcpy(nid.szTip, "HBLHOST");
				Shell_NotifyIconA(NIM_ADD, &nid);
			}
			return DefWindowProcA(hWnd, Msg, wParam, lParam);
		}
	};

	WNDCLASSA c;
	ZeroMemory(&c, sizeof(c));
	c.lpfnWndProc = Wnd::Proc;
	c.hInstance = GetModuleHandleA(NULL);
	c.lpszClassName = "HBLHOST";
	HWND hwnd = (RegisterClassA(&c) ? CreateWindowA(c.lpszClassName, 0, 0, 0, 0, 0, 0, 0, 0, c.hInstance, 0) : 0);
	if (!hwnd) return 1;

	MSG Msg;
	while (GetMessageA(&Msg, NULL, 0, 0) > 0) { TranslateMessage(&Msg); DispatchMessageA(&Msg); }
	return 0;
}

static void LogMsgBox(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	char* logbuf = (char*)malloc(1+vsnprintf(NULL, 0, format, ap));
	vsprintf(logbuf, format, ap);
	va_end(ap);
	MessageBoxA(0, logbuf, "HBLHOST", MB_ICONSTOP);
	free(logbuf);
}

#ifdef _DEBUG
#define LogText(...) LogFile(stdout, __VA_ARGS__)
#else
#pragma comment(linker, "/subsystem:windows")
#define LogText(format, ...) (void)0
#endif
#define LogError(format, ...) LogMsgBox(format, __VA_ARGS__)
#else //HBLHOST_USE_WIN32SYSTRAY
#define LogText(...) LogFile(stdout, __VA_ARGS__)
#define LogError(...) fprintf(stderr, __VA_ARGS__),fprintf(stderr, "\n\n")
#endif //HBLHOST_USE_WIN32SYSTRAY

#if !defined(HBLHOST_USE_WIN32SYSTRAY) || defined(_DEBUG)
static void LogFile(FILE* f, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	char* logbuf = (char*)malloc(1+vsnprintf(NULL, 0, format, ap));
	va_start(ap, format);
	vsprintf(logbuf, format, ap);
	va_end(ap);
	time_t RAWTime;
	time(&RAWTime);
	tm* LOCALTime = localtime(&RAWTime);
	fprintf(f, "[%02d:%02d:%02d] %s\n", LOCALTime->tm_hour, LOCALTime->tm_min, LOCALTime->tm_sec, logbuf);
	free(logbuf);
}
#endif

static void ShowHelp()
{
	LogError("HBLHost - Command Line Arguments" "\n\n"
		"-b <ip-address>   Specify the ip address of which interface to listen on (defaults to all interfaces)" "\n\n"
		"-p <port-num>     Specify the port number on which to serve web requests (defaults to 5000)" "\n\n"
		#ifndef HBLHOST_TEXTRESPONSEONLY
		"-e <elf-path>     Override the default elf executable path" "\n\n"
		#endif
		"-h                Show this help");
}

int main(int argc, char *argv[])
{
	const char *BindAddress = NULL, *CustomExecuteELF = NULL;
	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] != '-' && argv[i][0] != '/') continue;
		if      (argv[i][1] == 'b' && i < argc - 1) BindAddress = argv[++i];
		else if (argv[i][1] == 'p' && i < argc - 1) { g_Port = atoi(argv[++i]); if (g_Port <= 0) g_Port = 5000; }
		else if (argv[i][1] == 'e' && i < argc - 1) CustomExecuteELF = argv[++i];
		else if (argv[i][1] == 'h' || argv[i][1] == '?') { ShowHelp(); return 0; }
	}

	#ifndef HBLHOST_TEXTRESPONSEONLY
	g_MP4ExecuteELF = (char*)&mp4[14678 + 15]; //551
	inflate_mp4();
	if (CustomExecuteELF && CustomExecuteELF[0])
	{
		int lenCustom = (int)strlen(CustomExecuteELF), lenMax = (int)strlen(g_MP4ExecuteELF);
		if (lenCustom > lenMax)
		{
			LogError("Unable to use '%s' as an execute elf path as its length of %d characters is longer than the maximum %d characters - Aborting", CustomExecuteELF, lenCustom, lenMax);
			return EXIT_FAILURE;
		}
		strcpy(g_MP4ExecuteELF, CustomExecuteELF);
	}
	#else
	(void)CustomExecuteELF; //mark unused, avoid compiler warning
	#endif

	sts_net_socket_t server;
	if (sts_net_init() < 0 || sts_net_listen(&server, g_Port, BindAddress) < 0)
	{
		if (BindAddress) LogError("Webserver could not listen on %s:%d - Aborting", BindAddress, g_Port);
		else LogError("Webserver could not listen on port %d - Aborting", g_Port);
		return EXIT_FAILURE;
	}

	#ifdef HBLHOST_USE_WIN32SYSTRAY
	CreateThread(NULL, 0, SystrayThread, NULL, 0, NULL);
	#endif

	if (BindAddress)
	{
		strcpy(g_AddressTable[0].address, BindAddress);
		g_AddressCount = 1;
	}
	else
	{
		g_AddressCount = sts_net_enumerate_interfaces(g_AddressTable, 20, 1, 0);
		if (g_AddressCount > 20) g_AddressCount = 20;
	}

	for (int i = 0; i < g_AddressCount; i++)
		LogText((g_Port == 80 ? "Serving on http://%s/" : "Serving on http://%s:%d/"), g_AddressTable[i].address, g_Port);

	#ifdef HBLHOST_TEXTRESPONSEONLY
	LogText("Ready! Serving plain text response on every request...\n");
	#else
	LogText("Ready! Serving HBL MP4 launching %s on every request...\n", g_MP4ExecuteELF);
	#endif

	for (;;)
	{
		sts_net_socket_t client;
		if (sts_net_accept_socket(&server, &client) < 0) { LogError("Unknown error while accepting new connection"); continue; }

		char clientname[64] = {0,0};
		sts_net_gethostname(&client, clientname, sizeof(clientname), 1, NULL);
		LogText("Client connected '%s'!", clientname);

		char Request[2048];
		int RequestSize = (sts_net_check_socket(&client, 1.f) ? sts_net_recv(&client, Request, sizeof(Request)-1) : 0);
		//LogText("    %.*s", (RequestSize > 0 ? (strstr(Request, "\r\n\r\n") ? strstr(Request, "\r\n\r\n") - Request : RequestSize) : 0), Request);
		Request[RequestSize > 0 ? RequestSize : 0] = '\0'; //zero terminate for strstr
		if (RequestSize < 7 || !strstr(Request, "\r\n\r\n") || strstr(Request, "/favicon"))
		{
			//ignore failed connections, illegal http requests, favicon requests
		}
		#ifdef HBLHOST_TEXTRESPONSEONLY
		else
		#else
		else if (g_ServeTEXT)
		#endif
		{
			LogText("    Serving plain text response");
			sts_net_send(&client, s_ResponseTEXT, sizeof(s_ResponseTEXT) - 1);
		}
		#ifndef HBLHOST_TEXTRESPONSEONLY
		else if (!strstr(Request, "/MP4"))
		{
			LogText("    Root request sending redirect");
			sts_net_send(&client, s_ResponseREDIRECT, sizeof(s_ResponseREDIRECT) - 1);
		}
		else if (!strstr(Request, "Accept: */*"))
		{
			LogText("    Initial request from outside video player, sending only header");
			sts_net_send(&client, s_ResponseMP4, sizeof(s_ResponseMP4) - 1);
		}
		else
		{
			LogText("    Actual video request from the video player, serving mp4");
			sts_net_send(&client, s_ResponseMP4, sizeof(s_ResponseMP4) - 1);
			sts_net_send(&client, mp4, sizeof(mp4));
		}
		#endif //HBLHOST_TEXTRESPONSEONLY

		LogText("    Disconnecting client");
		sts_net_close_socket(&client);
	}
}

#ifdef HBLHOST_USE_WIN32SYSTRAY
int WINAPI WinMain(HINSTANCE, HINSTANCE,LPSTR, int)
{
	return main(__argc, __argv);
}
#endif

#ifndef HBLHOST_TEXTRESPONSEONLY
static void rle_inflate(const unsigned char* Data, unsigned char* OutBuf, size_t OutSize)
{
	for (unsigned char bits = 0, code = 0, *Out = OutBuf, *OutEnd = Out + OutSize; Out < OutEnd; code <<= 1, bits--)
	{
		if (bits == 0) { code = *Data++; bits = 8; }
		if ((code & 0x80) != 0) { *Out++ = *Data++; continue; }
		int RLE1 = *Data++, RLE = (RLE1<<8|*Data++), RLESize = ((RLE >> 12) == 0 ? (*Data++ + 0x12) : (RLE >> 12) + 2), RLEOffset = ((RLE & 0xFFF) + 1);
		while (RLESize > RLEOffset) { memcpy(Out, Out - RLEOffset, RLEOffset); Out += RLEOffset; RLESize -= RLEOffset; RLEOffset <<= 1; }
		memcpy(Out, Out - RLEOffset, RLESize); Out += RLESize;
	}
}

static const unsigned int mp4_deflated[] = {
	255, 0x79746618, 0x6733FF70, 13936, 0x69FE0001, 862809971, 0xB105367, 0x6D0090FD, 276197231, 0x64DE6C1F, 0xC9004000, 538607963, 0xFAFF03, 22530, 270281986, 0x9002202E, 0xF1FFFF01, 482348960, 0xB04000DF,
	0x6F69152B, 545223524, 462937, 687865679, 0xFF03FF, 0x74008800, 0x6B61DE72, 0x745C9310, 0x7620686B, 0x73600182, 0x77104D80, 0x846062E0, 0xA770008, 0xC55010BF, 0x78740080, 0x60806733, 50395152, 0xFF1301FF,
	67052290, 0x4704FF33, 0xFF5705FF, 0xFF670600, 0x8FF7707, 0x9B09FF8B, 0xFFAB0AFF, 0xCFFBB0B, 0xDF0DFFCF, 0xEF0E7FFF, 18684, 40401925, 0x8038A6FF, 0x847C4800, 0x8014FF1A, 939524260, 83820676, 0x78EBA37F,
	0xFF00C038, 883260418, 61439024, 388015799, 457221008, 0xFF006338, 0xFF004204, 0xF2217CF0, 0x618014FF, 0x697C0400, 0x4EA6FF03, 2098304, 0xB0FF2D00, 0xB51A203C, 0xFFD12160, 318785592, 620771573, 4456703,
	0x804E0200, 0x7C20FF00, 0x94A60208, 0xF0FFFF21, 67158419, 0xFF00E193, 50691080, 0xB267C78, 3700979, 790835254, 0xFEA07CFF, 0xC17C782B, 277051443, 277069603, 2177059, 0x7F087C10, 536894240, 0xA682D87F,
	0x82F9FF7F, 0x6007CA6, 0xFF4CAC, 0x603C2C01, 0xBB60F0FF, 0x789F1063, 0xB50A683, 0x79DF7C32, 0xF11FA083, 618029920, 0x3C803CFF, 813981920, 0x8390FF80, 0x803C0000, 0x60F7E760, 269615236, 0x803C040B, 0x60F17CFD,
	279350148, 0x40550817, 589299747, 0xFF23500C, 0x57102F10, 0x50F02310, 789582883, 0xDE846000, 0x4710AC06, 0x4C803C18, 30870288, 475205676, 0x60E84710, 537050756, 0x803C2047, 0x5F108048, 0x6B1003AF, 545214500,
	0xFA18007C, 520125612, 0x408F50AC, 0x7CED0F60, 319816704, 0xC3108030, 0x717CEB12, 0xC3108B, 0x7CFF0B20, 0x808B70DD, 6324451, 0x8B6BE740, 0x90BF206C, 0xF813204B, 50336512, 318787016, 0x3BD0CC01, 0xCD71711,
	671159040, 2556005, 0x77A02CD8, 0x91EE7B91, 0xF119EE7B, 280445051, 0xD0C41123, 0xC81320B7, 38994035, 293853311, 0x4CBF81DF, 0x6400FD00, 0x3C086354, 0x7B016B12, 0x40425F22, 0xFF2F0C00, 0xFFCBFF4B, 307176184,
	0x5322D04B, 31432764, 0xBB1280, 671138239, 31473663, 0xB8FF6302, 26279720, 0xE97F3400, 956278275, 945560741, 0xF51C00C1, 0x781B7E7C, 539074322, 0xC3E27F2B, 456161523, 521155344, 0xAA1F1063, 85655576, 339415157,
	0x84053700, 279533328, 0x96055300, 0x600C7310, 0x87F106F, 0x9F871043, 0xA7207720, 0x8B209720, 0xF42181, 0x70297D08, 0xA320180B, 0x40BD8313, 0x60381320, 592445441, 0xA730761C, 536871328, 0x407F7C2B, 990937031,
	0xFBE47F10, 0xDEA03878, 0xA3130300, 0xE03800, 539434800, 0xC5B50DB, 0x5F90E720, 0xBFE37F14, 0x3C0F70FB, 0x60FF1F60, 0xFFFFFF63, 0xA1FEFF4B, 0xFD6139, 385894448, 0xD89B13F4, 0x3DA3237F, 0xBF020120, 486473889,
	683157857, 0x9F2BBE7C, 291447160, 0x9D7C2C4F, 0x7CE47823, 0x533033DF, 547830609, 0xFDCB60DB, 0x78EBA47F, 0x7340C57F, 283542655, 0x60FC1F9B, 17502439, 455090016, 284647467, 0x7B10287B, 0x817BB074, 806921472,
	0x7B80BD7B, 817481489, 275226367, 825482371, 0x7C572147, 0x78FF3BFF, 775986263, 0xFF80BD57, 0xFB9F7F1E, 0xC0DE5778, 0xFF7F0EF7, 0x82361EB, 0xF385FF7F, 456230704, 0x8FB02F11, 378003684, 0x9F14E8AA, 408146656,
	0x52245392, 567251539, 0xE953B2FB, 0x53527B41, 0xE2FFB1DC, 854727845, 0x91FB5103, 0x9B6008A3, 554656271, 0x922B21DF, 0x647C1017, 0x3C78F31B, 664859775, 0x603CFB31, 0xC13B052, 0xA00C4BD0, 551752007, 279109392,
	300450645, 325791843, 0xD7311C27, 0x782BBCF0, 585831739, 0x42272247, 0x7161773, 588405522, 0x7F136065, 0x78FFE385, 0x78EBA67F, 0x9FFDFF4B, 307176149, 0xDE37D13F, 0x40F7FFFF, 315162530, 4724807, 27530709,
	331350851, 0x4351DE97, 0x972368C9, 0x43310A1D, 278369151, 813395991, 0x43913341, 0x4371171D, 0x4320C712, 17631075, 0x53330A03, 0xEB154FC1, 0x77E100F4, 815694396, 0xA0777113, 269705727, 0x7505DF3, 520150446,
	659569925, 343948477, 0x47217267, 556473732, 0x89671347, 349209409, 0xE007578C, 0xBB246723, 0xF176723, 0xB241C8C, 0x97446322, 0xDF32431D, 7008083, 0xCDFC048F, 5085008, 0x50AD051F, 0xA42D90B7, 0x887506B,
	0x3F25EF63, 999681827, 992291973, 523628323, 85224259, 0xB41D0843, 0x80B0549, 0xC7D1581D, 0xA50F2567, 0x9F643D83, 941653784, 0xFA30360, 324352788, 0x60382735, 0x61134510, 957401344, 0x6F246918, 0x7177187D,
	66584231, 268450048, 436224003, 0xC279545, 0xA734F7A4, 405735317, 0x9EE54013, 0xFF0F2800, 0xC873160E, 0xF812AB44, 0xA538C8, 696197121, 0xFF931613, 0x48037D1C, 0x48447DAE, 0x887FAEF7, 340467792, 0xD32939,
	0xE8A31601, 0xD320FF44, 808386816, 339197959, 0x437D3320, 0x504BAE49, 597291055, 0x937E9F43, 0xDF2BBF9F, 0x53223F78, 0x4100E09F, 0x87142B9E, 381944611, 0x8B23A5C7, 540963327, 0x5C1710C7, 67174203, 0xDCFF1F17,
	0x840F803F, 0xFA0DE03F, 0x8A9C6380, 0x452BD00C, 815475536, 0x4F202B20, 0x8B329E40, 0x60BF3C03, 404130594, 0x7C1D6338, 0x60077346, 858857274, 0xC411DF43, 0xE193C779, 0x43445F13, 0xE3DF2514, 0x5F003D33,
	384430869, 3408035, 0xBDE3F7C, 0x7FE36978, 645463009, 0x77164AC7, 278078992, 459963475, 0x6F352BF, 0xEB255320, 0x57265760, 0x3E7C57E0, 901331754, 0xC17F3B7A, 273887475, 0x4A6F28A3, 836828779, 0x780B24A7,
	0xA1FFBE, 16879860, 0x7CFD7C24, 613775387, 0x617B3300, 0xB4A329F8, 0x71AC383, 0x7337F710, 0x8E1D63EB, 406798184, 0xCFBD3B53, 858308635, 0xFF4B2F28, 35952119, 0xE72701F7, 85132252, 0x8D8BF23, 0x3F260573,
	0x4B03D4AA, 0xAF18A005, 90637264, 0xAACB18B2, 92472268, 0xC8E718BF, 0xC409E308, 0xAA09E308, 0x9D703C0, 0x9D703BC, 84870328, 0xA37319D5, 0xB0FF98B4, 0xFF231F77, 0x6138F353, 0x7C8800FF, 0x7CAA64BD, 0xAA65DFA3,
	0x800C7B1C, 0x75DC00A1, 956389142, 0x78B7155D, 0xF6610B19, 0xAA7C7C00, 0xE1FC1FE0, 413277968, 544226064, 0x501F6033, 990954779, 0x571024A9, 18546788, 0x55A767A9, 444457567, 0x777CAC17, 0xBF1C781B, 0xAB9951E0,
	0xF29B16A8, 387988266, 0x57C49F94, 402923293, 0xB3694CD3, 0xEF1F203D, 321478944, 0xB4FF40EF, 308912016, 403652847, 0x781B7B7C, 306811359, 538919167, 0xFFFF2039, 4874751, 0x3E2A911C, 0xB100F40, 0xB237C20,
	0xFF0F4078, 0x43952AFC, 0xA03CA400, 536875215, 97457499, 0xEF008138, 444169292, 805338375, 0xC4A3FE7C, 0xC5A47CAA, 0xEFDF1AAA, 360938496, 402686091, 0xFBFFEE4B, 874713313, 590378364, 593512583, 656416544,
	951919440, 0x6000DD9D, 0x7C2C1B1A, 0x847317A, 0x85371057, 0x502C1F50, 0x3C81990F, 0x873357D, 738247482, 0x5A3C5FB5, 0x4A207775, 0x55A0008B, 0x7F2ABC77, 450565726, 0x8460432B, 595056669, 0xA000FFF8, 51380281,
	0x7F15011E, 0x4E7D09CE, 0xBE5F450F, 0x5FD41C00, 0xF77AFBBF, 0x3DAC0041, 0x80FB1B00, 0x8000861, 956420885, 738216703, 335552145, 4124561, 0x91A72B84, 0xFF98005E, 1081400, 0xC00FE93, 4101117, 272601472,
	0xDF3F1F, 0xB43B1B6C, 0x44005F91, 20986357, 0x94131000, 0xF3D44B1B, 939532177, 386944811, 0x5FFF9368, 0x3F932400, 0xEF922800, 271843519, 0xDF92640B, 0x5B74AA00, 0xDF48D473, 0xB877A910, 0xB0882BDB, 587611051,
	408097050, 0xF2077BB, 0x3BF7771B, 0x7A9C00E1, 0x8C003DF3, 877231614, 895450538, 0x6B3F722, 0x97F6769, 0x5B42FF48, 0x61FFEAFF, 0x8130AF29, 0xFD98005D, 0x44003B91, 0xE310A138, 0x9400DF3D, 0x91088F1F, 0xB0980041,
	0x91DB1C7E, 269422625, 523460671, 522231311, 0x4B673A0F, 531757823, 0x80EAFF1F, 0x6048009B, 815183203, 0xE71B0F10, 0xB031A01, 0xB9875FE8, 0x61F737C0, 0x9B292C00, 524562205, 0x5F2C9F29, 0x9C7C9F29, 864057379,
	592306975, 0x432DD82A, 0x7F80A7A9, 0xE51B7049, 465584061, 0xB03FBDFD, 295054135, 465584061, 725601063, 624913213, 0xFB997720, 0xDB3FAD1C, 0xFBA9184B, 0xF4BEC03F, 0x9F236778, 0xDB52032A, 0xBF2C7F78, 0x3E7BEB07,
	0x60A3CB23, 0x3FA38B13, 723459072, 0xA08F278B, 0xDB647F13, 0xA3C37F78, 0xB83ADEB, 658312099, 0x867F775B, 0x674DE389, 721435448, 0x73671003, 322998148, 665789246, 0xAC249B10, 0x40EB1D63, 0x900FF48, 16793084,
	0xE3FF3CFF, 0x8610040, 0xFE7FFFFF, 0x40404087, 388759709, 319840175, 0x4113703E, 0x3D28009D, 338829531, 335594183, 0xEF6112E7, 419365194, 540873739, 0xDB145F30, 51597504, 0x82F87CFF, 427665135, 0x83580310,
	968261030, 0x4539BF8F, 0x497D2749, 0x49FDEF58, 0x497DAE20, 0x54EF7819, 0xC71469BF, 0x7F142A63, 0x40EF1889, 0x5F1A9C40, 0x7CAC4800, 0xAC0400EB, 522207001, 0xA07CECE3, 424612088, 0xF35F4F80, 0xFF29BF32,
	323333928, 0xCB670803, 0x3F15CB2C, 734742284, 0xA3454F99, 683206439, 0xE715F72F, 0x8B2F340C, 0x5A2BBF8E, 545294319, 0x8D8B2F43, 816000776, 0xBF0F3E6F, 0x9F6B1E41, 779690947, 782708483, 0x7C872E9B, 0x783BFFFB,
	0x78431A7D, 0xDF8241, 463396, 0x457F2A6D, 0x4C8A2FF3, 302459462, 0xFE6D1017, 788660266, 420741258, 0x3C149DB3, 0x84232E13, 0xBB1B6E2B, 0xFF80FB7D, 523837440, 0x7F1FA123, 989850389, 0x3BE97C14, 0x8B7FF778,
	0xB110040, 0xEFDFA060, 377565696, 0xC61C43, 0xC77CFD28, 0xA6801432, 0x6FAA6F1E, 0xEBAA2B11, 338299674, 0xFFFFA538, 363775, 0x7F802F01, 0x8820F739, 788529354, 54164870, 813727508, 0x60D31A40, 709563706,
	0x4A89DC87, 445282078, 0xBEEF1F, 0x48CB1924, 0xE8009A2F, 820001536, 0xC73B480F, 0x5716B71F, 0xE719EDBA, 983248696, 0x7B100C47, 0x5B910CEB, 0x709C5310, 0xF991140F, 0x8300005C, 459221481, 460614527, 671099139,
	777000731, 0xD3C7495B, 0x5F5EA007, 987446368, 0x55B4A34F, 530072351, 0x9751A4C3, 0x94E3167F, 0x98536F2A, 0x87178C23, 0x8C1B5494, 0x5F50672F, 0x8B5E8CC3, 369310780, 538403019, 0xA72F1F2F, 0x93120715, 691634177,
	0x9014D51E, 0x4391E311, 0x63140147, 0xA71F5793, 0x904B9090, 0x81940752, 0xA71F2429, 548217664, 474682659, 604579584, 801280431, 544413171, 0x4B609B60, 717868884, 452986661, 0xFF1FDF05, 87490596, 0xAB071FEC,
	89325608, 740499454, 0x3C180B91, 0xA060E9, 348044220, 0xF5DF37A3, 4723193, 0xE3684401, 0xA22C4B81, 0x70671F04, 465593215, 7081763, 363070747, 6856479, 0x9F250537, 0x409B7F64, 523871591, 0x7B6044CF, 0x9F709B5F,
	0x5D79649B, 0x603840CB, 530808577, 0x40CF79FF, 672875426, 746862759, 0xFF4BF348, 0xF5E61F9, 99103, 0x68BFDD77, 0x571FE47F, 0xA0300A0, 0xFF2F9B6F, 0xF35F8F2E, 0x44CBD76C, 662437805, 0xB7636013, 0xF32347E8,
	0xF507A8D, 343948524, 796725024, 0x932F0D03, 0x8F22072F, 0x762CE38E, 0x56888F1F, 0x85054701, 0x8E84AB1F, 529804515, 0x87B114BB, 0xE34EAF66, 484683328, 0xDF2EE36E, 337092921, 0x9E2F0C53, 530661177, 0xFF4BF4A9,
	52380152, 676279432, 310701003, 33234780, 0x40FE2337, 268640018, 445073667, 0x67A187D9, 0xA8D31CE5, 0x84771FF9, 0xF801FDF, 0x6F28261F, 0x60FFE324, 955559523, 0xF8A41184, 770899787, 801578624, 0x77AF2FF7,
	765845297, 0x78FF1F19, 0xD4E138, 19864188, 410829592, 0xFDC31EC7, 0x6A7C99FA, 790657307, 0xA2EF41, 0x7C931B18, 0xBD52847C, 0x78131C14, 532702012, 0xB9A5E77F, 520178432, 0x4B10DB22, 8067700, 0x740F1957,
	409998105, 0x647C4DFB, 0xA23F1174, 0xDB1B1C00, 485699444, 544259419, 579435591, 0x6D4B1014, 26673194, 275280680, 0x4B506C97, 0xAA5F196C, 21436776, 0x506C4B90, 0x4B90684B, 0xE30021A5, 0xB232D01, 0x5064E310,
	425026711, 0xD71244AB, 0x649790B5, 0x6F449750, 0xD5E69780, 951010177, 0xFD8438, 0xF0FF4B58, 0xE8D719E9, 0xFF803CFF, 0x4D6360F0, 0x8460F540, 319869987, 0x511380D5, 0xC1136054, 0x60552780, 0x9341AD27, 0xABD53B20,
	0x993B5030, 272912272, 273282127, 0x4F908563, 0x90711360, 0x5D27604F, 877629524, 273234768, 0xAF8329B7, 0x61EB335C, 0x934A4400, 50008512, 0x6360F526, 0xDB1C0414, 76749924, 0x4B0F217F, 0x3DE9E5FF, 274390560,
	320651423, 521199296, 0x9C1FD000, 540745929, 325279795, 521999145, 0x40B5E5CD, 0x63871B4F, 0x994FE013, 544425534, 0x60B4A34F, 0xF221563, 0xFF674495, 457463581, 0x571B482F, 694285032, 0xA720704C, 0x40503F1B,
	0x7050D50F, 0x40544B1B, 0xB20541B, 0x7F4B6A58, 0x631B60AA, 0xAE8B4B5C, 377359136, 0x610510D7, 457236521, 273461115, 0xB32925C3, 29515836, 0x60D7207A, 376227171, 0xE7201857, 19486206, 0x482965BA, 61355817,
	602206016, 52242539, 724611881, 545234005, 0xDC20E3D4, 0xC3D92C8F, 0x87197FA0, 0xFFCB8144, 0xFFEB81B8, 0xB82BCFF, 729989375, 0x82C4FFFF, 0x82C8FF4B, 0xCCFFFF6B, 0xD0FF8B82, 0xFFFFAB82, 0xFFCB82D4, 0xFFEB82D8,
	0xB83DCFF, 730063103, 0x83E4FFFF, 0x83E8FF4B, 0xECFFFF6B, 0xF0FF8B83, 0xFFFFAB83, 0xFFCB83F4, 0xFD0B80F8, 0xEB830400, 0xEF28FCFF, 0x5B61CF7D, 0x9F147B1C, 0x44003200, 33554666, 0x90330B40, 395331339, 594561173,
	319307551, 991497447, 925443604, 806371097, 0x830F1017, 0x49CB1FE1, 730024729, 0xBA1750BF, 0x67191314, 726681148, 476539192, 36376715, 0xEF5F39A6, 695739295, 0x5652446B, 0x5841DD48, 602970138, 0xF40310D4,
	0x6D656DFF, 7628147, 0x4153FF4F, 0x636F6C6C, 0x6FFF7246, 0x7379536D, 0x7E6D6574, 0x72461110, 0x6F546565, 0x49FF0E50, 0x65535F4D, 0xFF654474, 0x65636976, 0x74617453, 287335869, 0x736F6C43, 0xED4F0830, 275670384,
	274944835, 0x50FF6E32, 0x6F467475, 0xBF45746E, 0x43117078, 0x7261656C, 0x6675FB42, 275932518, 0xFF434415, 0x73756C46, 0x6E615268, 0x706567DE, 0x696C4634, 0xBB214070, 0x47389073, 910193765, 0x7ADE6953,
	0x49509065, 0x7074696E, 0x60539F6F, 0x44713024, 0x65766972, 0x4B2072FF, 0x656E7265, 0x4520FD6C, 0x6F6C7078, 0xDD5F2B10, 825260101, 275083843, 0x68AF54BB, 274990608, 0x736552F4, 0x806DB775, 0x4073490E,
	0x72655419, 0x6E696DEF, 6611984, 0x64FF6147, 544499047, 0xFF746F6E, 0x756F6620, 2188398, 846751743, 0x6C70722E, 0x5F5FFB00, 809717840, 0xFE434456, 0x61766E49, 291793260, 0xD5407E16, 0x6F544344, 0xE2706375,
	0xF91045B7, 0x97107463, 0xFE506F54, 0x69737968, 543973731, 0x92103B15, 0x58478950, 0x535D1132, 0x616DFE65, 0x726F6870, 0x7F0F2065, 0x46002331, 0x656C6961, 0x7420FB64, 811802735, 0x7F7420B9, 0x7F00BA40,
	21384261, 122114, 131184, 21896980, 75497719, 878059472, 0xCDD0000, 874262564, 353378304, 67172136, 523240192, 0x9D540000, 818880000, 0xB60B0003, 0xBA000340, 76160773, 722643068, 0xBFAB1EA9, 0xB0AF1EE8,
	0xBA7CA68A, 0x7CA6EF8B, 0xBB0710B1, 0xFB80A68B, 0x803400A3, 0xE3E219C3, 0x813CFE00, 0x81400003, 0xFE2F1323, 0x81480043, 508297315, 743488983, 27753631, 553590692, 0x420F5F1E, 0x8377527F, 0x571C00C4, 0xB33D3F7B,
	0xA1805B19, 791412772, 0x472B89B5, 0xB10F0FF, 537595946, 0xFF9E40FD, 528317668, 0xF2EDC433, 0x771F3B14, 0xAF193FA1, 726186908, 0x3E8184C3, 9016091, 0xA31901B3, 0xA719806C, 0x7F140F10, 0x404885FF, 0xB0FFBD41,
	998619695, 0x7E83548F, 373123867, 513718175, 0x3EDB63D7, 580877375, 523452197, 320869615, 0x7E101B10, 0xFC3EE7, 2781464, 270739201, 0x8018CB37, 990920464, 990342932, 16840604, 0x3F3DDE3B, 0xDF7F8378, 0x93202000,
	349863807, 0xDB7EEB7F, 815468664, 0xA1B09B50, 0x5F1C5F9F, 0x4A1D9F10, 0xFB7D2800, 0x8114525B, 0x4963104A, 0x7D14EE52, 0x5F03103F, 0xB31CAE50, 0x5B1A2EBF, 4819324, 0x8AE92B01, 0x671A6200, 738922520, 442921931,
	344548467, 671817431, 0x950F402B, 538660948, 856705063, 0x3C275003, 69668938, 857748241, 0xCE71030, 0x7E82C33F, 319923021, 0xC700E770, 0x4C5F2C05, 412611359, 0x5F1C632C, 0xFB5F4C68, 553597416, 563216383,
	0xFE90EC03, 0xBF1C0401, 452723489, 293125447, 0x997C00E3, 401809187, 81472344, 733629233, 0xB31D173A, 48123673, 950218132, 0x60800A60, 0x833A551F, 411253373, 0xAA04C71F, 0xE5483B, 0x3F2F6402, 0xBF30E79F,
	0xDB3FCF, 0xFFAF4F28, 0x3F83BAFF, 0x6D348800, 969617239, 591454359, 0xF3EB76B, 0x800060EF, 0x6338232E, 525696009, 0xAB1F3C57, 0xA1377038, 0xBDC1C31C, 3608579, 0xF0003980, 0x905B1C3B, 0xAF80953B, 0xEF1F7801,
	0xA0FC3C40, 0xC03C8000, 812334592, 0x9A5FF83, 0x9C638A6, 0xE1E938E8, 0x4BA0EC03, 0x5E61132C, 0x80223CB7, 0xC0731077, 0x531CAB80, 776263470, 0x6190F523, 0x93109803, 0x80C32E88, 0xEC03A1E9, 276878240, 0xBED7301B,
	940003199, 0x543F0063, 848560227, 0xEB7BEF2B, 485178235, 0xFB20172F, 0xEAF71067, 601331259, 65110079, 0x9C7F98FB, 52379704, 0x7FBD8138, 0xBB7C9F8E, 0xEF1B14E2, 0xFC7CFC01, 25251896, 0xDF3AC710, 0x6977115F,
	0xA1CB90, 0x40218128, 793263904, 865021715, 0x81972353, 0xDFA31DD3, 52042396, 0x799398FF, 0x9300C400, 723389231, 793557791, 0xCD72007, 0x7213F6C, 520222545, 587726857, 0x87FB1FF7, 0xD277BC2C, 0x925B324B,
	0xE3837F3B, 724508447, 0x61A71BF3, 4724740, 0x5F8404DE, 540897339, 0xE7371F00, 498991125, 0x94371F7F, 467630460, 0x7C2B2278, 0x8378239A, 0x4900C9DD, 0xA0A6327, 0xFB048B1F, 0xA603C97F, 0x3FEF113F, 0xADDEEB80,
	0x3E2B123E, 0xBD63F711, 0x630013FC, 568246428, 0x62F385B, 320998172, 0xC328532F, 319429455, 0x60BF731F, 0xF73A872A, 0xD63A700B, 0x5F290BD2, 727736431, 705307535, 0xB2946CF, 0xB72A08, 0xB71F0527, 593445386,
	660606090, 525367136, 350691145, 0x95051B00, 4140880, 525862199, 0x53001CC7, 525427973, 0x6F0020E3, 0x772F7705, 755337984, 4860083, 797574567, 96665719, 820453265, 0xAA05DF00, 887562143, 0xAB05FB00, 20506399,
	0xBAAA0517, 20768543, 533398835, 0x4F0140BF, 802395397, 90898803, 0x48F71FDD, 0xE8058701, 0x4CF71F55, 0xF605A301, 0xA150F71F, 0xA5D038BF, 0x545B2E0B, 0xDB710B6F, 0x7F22BF14, 0x5CD71B7E, 0xD0B6338, 0xE57B223F,
	0xA10413DE, 0x5C431FF7, 0xB70B3F4F, 0x583F1F18, 0x63CF143F, 0x4F5A14FF, 387464463, 0xB25E089, 0x422B5768, 761008935, 371403695, 0x780F8FDB, 951586623, 0x8010FF1F, 401821597, 0x76573F53, 75437871, 0xF8431EC3,
	0xEDFF1F39, 411013132, 0x7FC3036F, 275485683, 863933231, 0xFE1500A3, 416375680, 276258836, 0xD3270F26, 0x5F68E31E, 0x3F609F1C, 0x5C00A163, 0xD56193, 0x5C031060, 512865053, 0x7EFD60C3, 796439491, 817313156,
	528953120, 0x6B60CD3B, 789629750, 0x4F502356, 0x7E24FFFD, 528006115, 0x6F3B18F3, 270737387, 0xB193881, 0x78F6CB23, 0x9FD3447F, 553516899, 0xF2D0957, 0xF2DE72B, 277957421, 0x589F1F2B, 681287488, 589604719,
	656688928, 0x771C4B20, 0xAF230895, 479208348, 587998243, 0x53AF4646, 441314074, 0x6F537461, 0x6372FB75, 962994277, 0xF3002EF9, 0x6D204453, 275781152, 0x6FFD762F, 0x78652F6C, 0x61E01A74, 825257215, 0x6969772F,
	0x612FFF75, 796094576, 0x6DFF6F68, 0x65726265, 0xFE6C5F77, 0x636E7561, 7497064, 788463633, 6712421, 0x7F4E0072, 0x6E65CE1A, 0x6867756F, 1865248, 444166767, 438334172, 0x6620DD52, 3043098, 0x69841A63, 0x4D632B77,
	410799429, 0xFF666544, 0x746C7561, 0x70616548, 282794763, 0x90204C19, 0x4D1C0016, 0x79DC7063, 0x46547C00, 0xEA1A7461, 0x49BFF0BB, 0x5F5F2CFB, 0x735F736F, 0x72706EFF, 0x66746E69, 728086272, 995313320, 0x415346E1,
	0x436464FD, 0x3C65696C, 0x53F3464D, 0x406C6544, 0x4320500D, 0x4264D86D, 7058716, 0x40103DE1, 275925809, 0x6D6E5548, 0xA42C0950, 273554246, 0xC42C2D40, 475139408, 0x3C18500C, 0xE62430D1, 292780403, 0x53E93B54,
	0x77281C59, 0x69545341, 0x43401074, 0x6CDE756F, 0x6C133C64, 828662127, 18907955, 0xDC8B2187, 0x7E214630, 2215169, 0x6873FF2E, 0x74727473, 0xED6261, 387085358, 0x871C2E00, 43361, 0x5C0B1C00, 390858267,
	0xD500231C, 0x43907C09, 292051972, 0xD632435C, 522223616, 40573961, 0xAD6B903A, 50565121, 0xA0C9C8C, 1550364, 0xCD0D27D0, 0x6F20D04, 0x570EE5CD, 0xC4A0E02, 0xD991958E, 0xDA115FAF, 0x4D2C334C, 0xFD536969,
	0x69647574, 0xCB5D006F, 689111072, 0xC730D33, 794817853, 0xE241703, 503514121, 0xBA3E020E, 0x75AF646F, 0xC30F90DF, 787870479, 0x69FF1F1F, 0x6E612074, 0xCF722064, 0x65132D65, 0x7262381F, 0x7385776F, 739190804,
	0x64574F22, 941908306, 0x6974F061, 0xCC646E6F, 0xE0A6E00, 0xA20005E9, 0xEF52670B, 0xF1326F72, 0x622EF742, 0x4F73AD73, 0x90323C15, 0xD81E527A, 453115713, 0xD00010C, 0x41FF110B, 0x8F128E7F, 0xFF109011, 0xE920F91,
	0xC940D93, 0x960B95FF, 0x9809970A, 0x799FF08, 94045850, 0x9DF9049C, 0x9F029E03, 0x5916555F, 0xFF44FB00, 535621119, 0xFF005864, 0xCF41CE41, 0xD141D041, 0x41D241FF, 0x41D441D3, 0xD641FFD5, 0xD841D741, 0x41FFD941,
	0x41DB41DA, 0xFFDD41DC, 0xDF42DE41, 0x41410641, 69104, 0xFF000090, 50270465, 855899938, 0x440400FF, 0xFF5505FF, 0x7FF6606, 0x8808FF77, 0xFF9909FF, 0xBFFAA0A, 0xC04FFBB, 0xDD0DFFCC, 0xFFEE0EFF, 0xFFFFF0F,
	5823999, 285343488, 570556671, 0xFF3303FF, 0x5FF4404, 0x6606FF55, 0xFF7707FF, 0x9FF8808, 0xA07FF99, 0xBB0BFFAA, 0xFFCC0CFF, 0xEFFDD0D, 16908270, 2392269, 318897923, 0xFF2302FF, 83833603, 0x5705FF47, 0xFF6706FF,
	0xFF770700, 0x9FF8B08, 0xAB0AFF9B, 0xFFBB0BFF, 0xDFFCF0C, 0xEF0EFFDF, 0xFF0F00FF, 0xFFFF0FFF, 0xFFFFF0F, 0xFF0FFFFF, 0xFFFF0FFF, 0xFFFFF0F, 0x48BE78FF, 33619984, 51204, 33489152, 24173831, 0x5F503104,
	805444496, 0x74020817, 702022657, 70189276, 0x4700E418, 56557570, 0xE20A7700, 0xF0C07603, 7803023, 0xA0021347, 0xC85FF747, 0xDD408001, 0x93DD5614, 457637928, 16813824, 0xA1238F00, 0x8D3F024F, 0x708F6088,
	591855783, 0x4700B040, 65995042, 0xFF1F1816, 0xA3020180, 33666303, 33630392, 0x5160BDF1, 0x56140817, 0x801F41CD, 0xFF9F0120, 0x487F9F01, 33488896, 570621713, 0xF63303FF, 0xC5FBD917, 0x6169646D, 0x7874E227,
	661074911, 0x80FFFFEA, 0x95E08400, 50377894, 0xFF1301FF, 67052290, 0x4704FF33, 0x570500FF, 0xFF6706FF, 0x8FF7707, 0x9B09FF8B, 0xFFAB0AFF, 0xCFFBB0B, 0xD00FFCF, 0xEF0EFFDF, 0xFFFF0FFF, 0xFFFFF0F, 0xFF0FFFFF,
	0xFFFF0FFF, 0xFFFF0F, 0xFFFFF0F, 0xFF0FFFFF, 0xFFFF0FFF, 0xFFFFF0F, 0xFF0FFFFF, 0xFFFF0FFF, 0xFDFF0F00
};

static void inflate_mp4()
{
	rle_inflate((const unsigned char*)mp4_deflated, mp4, sizeof(mp4));
}
#endif

//#ifdef HBLHOST_USE_WIN32SYSTRAY
//#define WMSTR(wm){ wm, #wm }
//static const struct { UINT Msg; LPCSTR Str; } s_MsgStr[] = {
//	WMSTR(WM_CREATE), WMSTR(WM_COMMAND), WMSTR(WM_INITMENU), WMSTR(WM_INITMENUPOPUP), WMSTR(WM_MENUSELECT), WMSTR(WM_UNINITMENUPOPUP), WMSTR(WM_ACTIVATEAPP), 
//	WMSTR(WM_MOUSEMOVE), WMSTR(WM_ENTERIDLE), WMSTR(WM_SETCURSOR), WMSTR(WM_ACTIVATE), WMSTR(WM_ENTERMENULOOP), WMSTR(WM_EXITMENULOOP), WMSTR(WM_GETMINMAXINFO),
//	WMSTR(WM_CAPTURECHANGED), WMSTR(WM_LBUTTONUP), WMSTR(WM_LBUTTONDBLCLK), WMSTR(WM_RBUTTONDOWN), WMSTR(WM_RBUTTONUP), WMSTR(WM_WINDOWPOSCHANGING), 
//	WMSTR(WM_SETFOCUS), WMSTR(WM_KILLFOCUS),WMSTR(WM_NCCREATE), WMSTR(WM_NCCALCSIZE), WMSTR(WM_NCACTIVATE), {0,NULL}
//};
//const LPCSTR GetMsgString(UINT Msg) { for (int i = 0; s_MsgStr[i].Msg; i++) if (s_MsgStr[i].Msg == Msg) return s_MsgStr[i].Str; return ""; }
//#undef WMSTR
//#endif
