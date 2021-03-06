/*
 * menusetup.c: wirbelscan - A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */


#include <linux/dvb/frontend.h>
#include <vdr/menuitems.h>
#include <vdr/device.h>
#include <vdr/config.h>
#include <vdr/tools.h>
#include "menusetup.h"
#include "common.h"
#include "satellites.h"
#include "countries.h"
#include "wirbelscan.h"
#include "scanner.h"
#include "common.h"

using namespace COUNTRY;

static const char * ScannerDesc   = "wirbelscan scan thread";
static const char * DVB_Types[]  = {"DVB-T","DVB-C","DVB-S","pvrinput","pvrinput FM","ATSC", "no device found"};

cMenuScanning * MenuScanning    = NULL;   // pointer to actual menu
cScanner      * Scanner         = NULL;
cOsdItem      * DeviceUsed      = NULL;
cOsdItem      * Progress        = NULL;
cOsdItem      * CurrTransponder = NULL;
cOsdItem      * Str             = NULL;
cOsdItem      * ChanAdd         = NULL;
cOsdItem      * ChanNew         = NULL;
cOsdItem      * ChanAll         = NULL;
cOsdItem      * ScanType        = NULL;

#define LOGLEN 8
cOsdItem      * LogMsg[LOGLEN];


int     channelcount = 0;
int     lProgress    = 0;
int     lStrength    = 0;
cString lTransponder = NULL;
cString deviceName   = NULL;
time_t  timestamp;

class cMenuSettings : public cMenuSetupPage {
 private:
   int scan_tv;
   int scan_radio;
   int scan_fta;
   int scan_scrambled;
   int scan_hd;
 protected:
   void AddCategory(const char * category);
   void Store(void);
 public:
   cMenuSettings();
   ~cMenuSettings(void) {};
   virtual eOSState ProcessKey(eKeys Key);
};

cMenuSettings::cMenuSettings() {
  static const char *Symbolrates[] = {tr("AUTO"),"6900","6875","6111","6250","6790","6811","5900","5000","3450","4000","6950","7000","6952","5156","5483",tr("ALL (slow)")};
  static const char *Qams[]        = {tr("AUTO"),"64","128","256",tr("ALL (slow)")};
  static const char *logfiles[]    = {tr("Off"),"stdout","syslog"};
  static const char *inversions[]  = {tr("AUTO/OFF"),tr("AUTO/ON")};
  #if VDRVERSNUM > 10713
  static const char *atsc_types[]  = {"VSB (aerial)","QAM (cable)","VSB + QAM (aerial + cable)"};
  #endif
  static char *SatNames[256];
  static char *CountryNames[256];

  if (! wSetup.systems[DVB_TERR] &&
      ! wSetup.systems[DVB_CABLE] &&
      ! wSetup.systems[DVB_SAT] &&
      ! wSetup.systems[PVRINPUT] &&
      ! wSetup.systems[PVRINPUT_FM] &&
      ! wSetup.systems[DVB_ATSC]) {
    AddCategory("NO DEVICES FOUND.");
    return;
    }       

  scan_tv        = (wSetup.scanflags & SCAN_TV       ) > 0;
  scan_radio     = (wSetup.scanflags & SCAN_RADIO    ) > 0;
  scan_scrambled = (wSetup.scanflags & SCAN_SCRAMBLED) > 0;
  scan_fta       = (wSetup.scanflags & SCAN_FTA      ) > 0;
  scan_hd        = (wSetup.scanflags & SCAN_HD       ) > 0;

  for (int i=0; i < sat_count(); i++) {
    SatNames[i] = (char *) malloc(strlen(sat_list[i].full_name) + 1);
    strcpy(SatNames[i], sat_list[i].full_name);
    }
  for (int i=0; i < country_count(); i++) {
    CountryNames[i] = (char *) malloc(strlen(country_list[i].full_name) + 1);
    strcpy(CountryNames[i], country_list[i].full_name);
    }

  SetSection(tr("Setup"));
  AddCategory(tr("General"));
  Add(new cMenuEditStraItem(tr("Source Type"), (int *) &wSetup.DVB_Type,  6, DVB_Types));
  Add(new cMenuEditIntItem (tr("verbosity"),           &wSetup.verbosity, 0, 5));
  Add(new cMenuEditStraItem(tr("logfile"),             &wSetup.logFile,   3, logfiles));

  AddCategory(tr("Channels"));
  Add(new cMenuEditBoolItem(tr("TV channels"),        &scan_tv));
  Add(new cMenuEditBoolItem(tr("Radio channels"),     &scan_radio));
  Add(new cMenuEditBoolItem(tr("FTA channels"),       &scan_fta));
  Add(new cMenuEditBoolItem(tr("Scrambled channels"), &scan_scrambled));
  #if VDRVERSNUM > 10701
  Add(new cMenuEditBoolItem(tr("HD channels"),        &scan_hd));
  #endif

  if (wSetup.systems[DVB_CABLE] || wSetup.systems[DVB_TERR] || wSetup.systems[DVB_ATSC]) {
     AddCategory(tr("Cable and Terrestrial"));
     Add(new cMenuEditStraItem(tr("Country"),             &wSetup.CountryIndex, country_count(), CountryNames));
     if (wSetup.systems[DVB_CABLE]) {
        Add(new cMenuEditStraItem(tr("Cable Inversion"),  &wSetup.DVBC_Inversion,   2, inversions));
        Add(new cMenuEditStraItem(tr("Cable Symbolrate"), &wSetup.DVBC_Symbolrate, 17, Symbolrates));
        Add(new cMenuEditStraItem(tr("Cable modulation"), &wSetup.DVBC_QAM,         5, Qams));
        }
     if (wSetup.systems[DVB_TERR]) {
        Add(new cMenuEditStraItem(tr("Terr  Inversion"),  &wSetup.DVBT_Inversion,   2, inversions));
        }
     if (wSetup.systems[DVB_ATSC]) {
        #if VDRVERSNUM > 10713
        Add(new cMenuEditStraItem(tr("ATSC  Type"),       &wSetup.ATSC_type,        3, atsc_types));
        #endif
        }
     }

  if (wSetup.systems[DVB_SAT]) {
     AddCategory(tr("Satellite"));
     Add(new cMenuEditStraItem(tr("Satellite"),        &wSetup.SatIndex, sat_count(), SatNames));
     #if VDRVERSNUM <= 10700
     Add(new cMenuEditBoolItem(tr("DVB-S2"),           &wSetup.enable_s2));
     #endif
     }
}

void cMenuSettings::Store(void) {
  wSetup.scanflags  = scan_tv       ?SCAN_TV       :0;
  wSetup.scanflags |= scan_radio    ?SCAN_RADIO    :0;
  wSetup.scanflags |= scan_scrambled?SCAN_SCRAMBLED:0;
  wSetup.scanflags |= scan_fta      ?SCAN_FTA      :0;
  wSetup.scanflags |= scan_hd       ?SCAN_HD       :0;
  wSetup.update = true;
  }



void cMenuSettings::AddCategory(const char * category) {
  cString buf = cString::sprintf("---------------  %s ", category);
  cOsdItem * osditem = new cOsdItem(*buf);
  Add(osditem);
  }

eOSState cMenuSettings::ProcessKey(eKeys Key) {
  int direction = 0;
  eOSState state = cMenuSetupPage::ProcessKey(Key);
    switch(Key) {
      case kLeft:
           direction = -1;
           break; 
      case kRight:
           direction = 1;
           break; 
      case kOk:
      case kBack:
        thisPlugin->StoreSetup();
        wSetup.update = true;
        state=osBack;
        break;
      default:;
      }
  if (state == osUnknown) {
    switch(Key) {
      case kGreen:
      case kRed:
      case kBlue:
      case kYellow:
        state=osContinue;
        break;
      default:;
      }
    }

  while (! wSetup.systems[wSetup.DVB_Type] && wSetup.DVB_Type != NO_DEVICE) {
     //
     dlog(3, "%s unsupported", DVB_Types[wSetup.DVB_Type]);
     ProcessKey(direction < 0? kLeft:kRight);
     }

  return state;      
}


///! stopScanners() should be called only if plugin is destroyed.
void stopScanners(void) {
 if (Scanner) {
    dlog(0, "Stopping scanner.");
    Scanner->SetShouldstop(true);
    }
  }

///! DoScan(int DVB_Type), call this one to create new scanner.
bool DoScan(int DVB_Type) {
  if (Scanner && Scanner->Active())
     return false;

  wSetup.InitSystems();
  if (DVB_Type == NO_DEVICE || ! wSetup.systems[DVB_Type] ||
     (DVB_Type == PVRINPUT    && ! wSetup.enable_pvrinput) ||
     (DVB_Type == PVRINPUT_FM && ! wSetup.enable_pvrinput))
     return false;

  timestamp = time(0);
  channelcount = Channels.Count();
  Scanner = new cScanner(ScannerDesc, (scantype_t) DVB_Type);
  return true;
}

void DoStop(void) {
  if (Scanner && Scanner->Active())
     Scanner->SetShouldstop(true);
}

cMenuScanning::~cMenuScanning(void) {
  MenuScanning = NULL;
}

cMenuScanning::cMenuScanning(void) {
  static const char * Buttons[] = { tr("Stop"), tr("Start"), tr("Settings"), /*tr("About")*/ "" };
  char * buf;

  /* the four color buttons */
  SetHelp(Buttons[0], Buttons[1], Buttons[2], Buttons[3]);
  MenuScanning = this;
  needs_update = true;

  wSetup.InitSystems();

  #ifdef OSD_DBG
  memset(&systems,1,sizeof(systems));
  #endif

  if (! wSetup.systems[DVB_TERR] &&
      ! wSetup.systems[DVB_CABLE] &&
      ! wSetup.systems[DVB_SAT] &&
      ! wSetup.systems[PVRINPUT] &&
      ! wSetup.systems[PVRINPUT_FM] &&
      ! wSetup.systems[DVB_ATSC]) {
    AddCategory("NO DEVICES FOUND.");
    return;
    }

  AddCategory(tr("Status"));
  if (0 > asprintf(&buf, "%s %s %s",
      DVB_Types[wSetup.DVB_Type],
      (wSetup.DVB_Type == DVB_SAT)?
       sat_list[wSetup.SatIndex].full_name:country_list[wSetup.CountryIndex].full_name,
       "STOP"))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  ScanType = new cOsdItem(buf);
  DELETENULL(buf);
  Add(ScanType);

  if (0 > asprintf(&buf, "Device ?"))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  DeviceUsed = new cOsdItem(buf);
  DELETENULL(buf);
  Add(DeviceUsed);
  
  if (0 > asprintf(&buf, "Scan: %d%%", 0))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  Progress = new cOsdItem(buf);
  DELETENULL(buf);
  Add(Progress);

  if (0 > asprintf(&buf, " "))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  CurrTransponder = new cOsdItem(buf);
  DELETENULL(buf);
  Add(CurrTransponder);

  if (0 > asprintf(&buf, "STR   0%% []"))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  Str = new cOsdItem(buf);
  DELETENULL(buf);
  Add(Str);

  AddCategory(tr("Channels"));

  if (0 > asprintf(&buf, "%s (%s, %s)",
    (wSetup.scanflags & (SCAN_TV | SCAN_RADIO)) == SCAN_TV?"TV only":
    (wSetup.scanflags & (SCAN_TV | SCAN_RADIO)) == SCAN_RADIO?"Radio only":
    (wSetup.scanflags & (SCAN_TV | SCAN_RADIO)) == (SCAN_RADIO | SCAN_TV)?"TV + Radio":
    "don''t add channels",
    (wSetup.scanflags & (SCAN_HD)) == SCAN_HD?"SDTV + HDTV":"SDTV",
    (wSetup.scanflags & (SCAN_FTA | SCAN_SCRAMBLED)) == SCAN_FTA?"Free to Air only":
    (wSetup.scanflags & (SCAN_FTA | SCAN_SCRAMBLED)) == SCAN_SCRAMBLED?"Scrambled only":
    (wSetup.scanflags & (SCAN_FTA | SCAN_SCRAMBLED)) == (SCAN_FTA | SCAN_SCRAMBLED)?"Free to Air + Scrambled":
    "don''t add channels"))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  ChanAdd = new cOsdItem(buf);
  DELETENULL(buf);
  Add(ChanAdd);

  if (0 > asprintf(&buf, "new Channels:"))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  ChanNew = new cOsdItem(buf);
  DELETENULL(buf);
  Add(ChanNew);

  if (0 > asprintf(&buf, "all Channels: %d", Channels.Count()))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  ChanAll = new cOsdItem(buf);
  DELETENULL(buf);
  Add(ChanAll);

  AddCategory(tr("Log Messages"));

  if (0 > asprintf(&buf, " "))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  for (int i = 0; i < LOGLEN; i++) {
    LogMsg[i] = new cOsdItem(buf);
    Add(LogMsg[i]);
    }
  DELETENULL(buf);
}

void cMenuScanning::SetChanAdd(uint32_t flags) {
  char * buf;
  if (0 > asprintf(&buf, "%s (%s, %s)",
    (flags & (SCAN_TV | SCAN_RADIO)) == SCAN_TV?"TV only":
    (flags & (SCAN_TV | SCAN_RADIO)) == SCAN_RADIO?"Radio only":
    (flags & (SCAN_TV | SCAN_RADIO)) == (SCAN_RADIO | SCAN_TV)?"TV + Radio":
    "don''t add channels",
    (flags & (SCAN_HD)) == SCAN_HD?"SDTV + HDTV":"SDTV",
    (flags & (SCAN_FTA | SCAN_SCRAMBLED)) == SCAN_FTA?"Free to Air only":
    (flags & (SCAN_FTA | SCAN_SCRAMBLED)) == SCAN_SCRAMBLED?"Scrambled only":
    (flags & (SCAN_FTA | SCAN_SCRAMBLED)) == (SCAN_FTA | SCAN_SCRAMBLED)?"Free to Air + Scrambled":
    "don''t add channels"))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  ChanAdd->SetText(buf, true);
  DELETENULL(buf);
  ChanAdd->Set();
  MenuScanning->Display();
}

void cMenuScanning::SetStatus(int status) {
  char * buf;
  int type = Scanner?Scanner->DvbType() : wSetup.DVB_Type;
  const char * st[] = {"STOP", "RUN", "No device available - exiting!", "No DVB-S2 device available - trying fallback to DVB-S", " "};

  if (0 > asprintf(&buf, "%s %s %s",
      DVB_Types[type],
      (type == DVB_SAT)?
      sat_list[wSetup.SatIndex].full_name:country_list[wSetup.CountryIndex].full_name,
      Scanner? st[Scanner->Status()] : st[status]))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  ScanType->SetText(buf, true);
  DELETENULL(buf);
  ScanType->Set();
  MenuScanning->Display();
}

void cMenuScanning::SetProgress(const int progress, scantype_t type, int left) {
  char * buf;
  time_t t = time(0) - timestamp;
  double est = 0.0;
  if (progress > 2) {
    est = (100 - progress) * (t/progress);
    if (0 > asprintf(&buf, "Scan: %d%% running %dm%.2dsec (est: %dm%.2dsec left)",
        progress, (int) t/60, (int) t%60, (int) est/60, (int) est%60))
      dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
    }
  else
    if (0 > asprintf(&buf, "Scan: %d%% running %dm%.2dsec",
        lProgress, (int) t/60, (int) t%60))
      dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  Progress->SetText(buf, true);
  DELETENULL(buf);
  Progress->Set();
  if (needs_update) {
    SetStatus(0);
    SetDeviceInfo(NULL, false);
    needs_update = false;
    }
  MenuScanning->Display();
}

void cMenuScanning::SetTransponder(const cChannel * transponder) {
  char * buf;
  if (0 > asprintf(&buf, "%s", *PrintTransponder(transponder)))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  CurrTransponder->SetText(buf, true);
  DELETENULL(buf);
  CurrTransponder->Set();
  MenuScanning->Display();
}

void cMenuScanning::SetStr(uint strength, bool locked) {
  char * buf;
  int res;
  double s = ((int) strength * 100.0) / 65535;
  strength = (int) s;
  switch ((int) s /*strength*/) {
    case  0 ... 10: res=asprintf(&buf, "STR %-3d%% []           %s", strength, locked?"LOCKED":""); break;
    case 11 ... 20: res=asprintf(&buf, "STR %-3d%% [_]          %s", strength, locked?"LOCKED":""); break;
    case 21 ... 30: res=asprintf(&buf, "STR %-3d%% [__]         %s", strength, locked?"LOCKED":""); break;
    case 31 ... 40: res=asprintf(&buf, "STR %-3d%% [___]        %s", strength, locked?"LOCKED":""); break;
    case 41 ... 50: res=asprintf(&buf, "STR %-3d%% [____]       %s", strength, locked?"LOCKED":""); break;
    case 51 ... 60: res=asprintf(&buf, "STR %-3d%% [_____]      %s", strength, locked?"LOCKED":""); break;
    case 61 ... 70: res=asprintf(&buf, "STR %-3d%% [______]     %s", strength, locked?"LOCKED":""); break;
    case 71 ... 80: res=asprintf(&buf, "STR %-3d%% [_______]    %s", strength, locked?"LOCKED":""); break;
    case 81 ... 90: res=asprintf(&buf, "STR %-3d%% [________]   %s", strength, locked?"LOCKED":""); break;
    default:        res=asprintf(&buf, "STR %-3d%% [_________]  %s", strength, locked?"LOCKED":""); break;
    }
  if (res < 0) {
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
    return;
    } 
  Str->SetText(buf, true);
  DELETENULL(buf);
  Str->Set();
  MenuScanning->Display();
}

void cMenuScanning::SetChan(int count) {
  char * buf;
  if (0 > asprintf(&buf, "new Channels: %d", (Channels.Count() - channelcount)>0?Channels.Count() - channelcount:0))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  ChanNew->SetText(buf, true);
  DELETENULL(buf);
  ChanNew->Set();
  if (0 > asprintf(&buf, "all Channels: %d", Channels.Count()))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  ChanAll->SetText(buf, true);
  DELETENULL(buf);
  ChanAll->Set();
  MenuScanning->Display();
}

void cMenuScanning::SetDeviceInfo(cString Info, bool update) {
  char * buf;
  if (update)
    deviceName = cString::sprintf("%s", *Info);
  if (0 > asprintf(&buf, "Device %s", *deviceName))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  DeviceUsed->SetText(buf, true);
  DELETENULL(buf);
  DeviceUsed->Set();
  MenuScanning->Display();
}

void cMenuScanning::AddLogMsg(const char * Msg) {
  for (int i = 0; i < LOGLEN - 1; i++) {
    LogMsg[i]->SetText(LogMsg[i+1]->Text(), true);    
    LogMsg[i]->Set();
    }
  LogMsg[LOGLEN - 1]->SetText(Msg, true);
  LogMsg[LOGLEN - 1]->Set();
  MenuScanning->Display();
}

void cMenuScanning::AddCategory(const char * category) {
  char * buf=NULL;
  if (0 > asprintf(&buf, "---------------  %s ", category))
    dlog(0, "%s (%d): could not allocate memory", __FUNCTION__, __LINE__);
  cOsdItem * osditem = new cOsdItem(buf);
  Add(osditem);
  DELETENULL(buf);
}

eOSState cMenuScanning::ProcessKey(eKeys Key) {
  if (wSetup.update) {
    SetStatus(4);
    SetChanAdd(wSetup.scanflags);
    wSetup.update = false;
    }
  eOSState state = cMenuSetupPage::ProcessKey(Key);
  switch (Key) {
    case kUp:
    case kDown:
      return osContinue;
    default:;
    }
  if (state == osUnknown) {
    switch(Key) {
      case kBack:
      case kOk:    state=osBack;
                   return state;
                   break;
      case kGreen:
                   if (wSetup.systems[DVB_TERR] ||
                       wSetup.systems[DVB_CABLE] ||
                       wSetup.systems[DVB_SAT] ||
                       wSetup.systems[PVRINPUT] ||
                       wSetup.systems[PVRINPUT_FM] ||
                       wSetup.systems[DVB_ATSC]) {
                     state=osContinue;
                     needs_update = true;
                     StartScan();
                     }
                   break;
      case kRed:
                   if (wSetup.systems[DVB_TERR] ||
                       wSetup.systems[DVB_CABLE] ||
                       wSetup.systems[DVB_SAT] ||
                       wSetup.systems[PVRINPUT] ||
                       wSetup.systems[PVRINPUT_FM] ||
                       wSetup.systems[DVB_ATSC]) {
                     state=osContinue;
                     needs_update = true;
                     StopScan();
                     }
                   break;
/*      case kBlue:  return AddSubMenu(new cMenuAbout());
                   break;*/
      case kYellow:
                   if (wSetup.systems[DVB_TERR] ||
                       wSetup.systems[DVB_CABLE] ||
                       wSetup.systems[DVB_SAT] ||
                       wSetup.systems[PVRINPUT] ||
                       wSetup.systems[PVRINPUT_FM] ||
                       wSetup.systems[DVB_ATSC]) 
                      return AddSubMenu(new cMenuSettings());
                   break;
      default:     break;
      }
    }
  if (Scanner && Scanner->Active() && (state != osBack))
      return osContinue;
  return state;      
}

bool cMenuScanning::StopScan(void) {
  DoStop();
  return true;
}

bool cMenuScanning::StartScan(void) {
  scantype_t type = wSetup.DVB_Type;

  dlog(0, "StartScan(%s)", DVB_Types[type]);

  if (! wSetup.systems[type]) {
     dlog(0, "Skipping scan: CANNOT SCAN - No device!");
     Skins.Message(mtInfo, tr("CANNOT SCAN - No device!"));
     sleep(6);
     return false;
     }

  if (ActiveTimers(type)) {
    dlog(0, "Skipping scan: CANNOT SCAN - Timers active!");
    Skins.Message(mtInfo, tr("CANNOT SCAN - Timers active!"));
    sleep(6);
    return false;
    }

  if (PendingTimers(type, time(NULL) + 7200)) {
    dlog(0, "Skipping scan: CANNOT SCAN - Timers on Schedule!");
    Skins.Message(mtInfo, tr("CANNOT SCAN - Timers on Schedule!"));
    sleep(6);
    return false;
    }

  return DoScan(type);
}

void cMenuScanning::Store(void) {
  thisPlugin->StoreSetup();
}
