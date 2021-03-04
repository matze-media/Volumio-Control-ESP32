#include <main.h>

void setup() {
  USE_SERIAL.begin(115200);
  USE_SERIAL.println("Setup volumio control");
  setCpuFrequencyMhz(80);
  sleepCount = 0;

  wakeup_reason = esp_sleep_get_wakeup_cause();
  USE_SERIAL.print("Wakeup Reason: ");
  USE_SERIAL.println(wakeup_reason); 
  
  u8g2.begin();  
  u8g2.clearBuffer();

  // init Button for play/pause
  pinMode(PIN_Encoder_SW,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_Encoder_SW), checkButton, CHANGE);

  // rotery encoder
  attachInterrupt(PIN_Encoder_CLK, checkPosition, CHANGE);
  attachInterrupt(PIN_Encoder_DT, checkPosition, CHANGE);

  // IR enable
  IRReceiver.enableIRIn();

  showSetup();
  // wait 4 seconds till internal wifi is ready
  for(uint8_t t = 3; t > 0; t--) {
      USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
      USE_SERIAL.flush();
      delay(1000);
  }
  
  connectWifi();
  u8g2.clearBuffer();
}

void loop() {
  
  unsigned long now = millis();

  // workaround for bug with long button press
  if (gotoSleep == true)
  {
    gotoSleep = false;
    goToDeepSleep();
    return;
  }

  while (wifiMulti.run() != WL_CONNECTED)
  {
    connectWifi();
  }

  // init IR remote decoder
  if (IRReceiver.decode(&ir_receiver_result)) {
    unsigned int ir_code = ir_receiver_result.value;
    USE_SERIAL.print("IR Remote: ");
    USE_SERIAL.println(ir_code, DEC);
    interpretIR(ir_code);
    IRReceiver.resume();
  }

  //Check if Volumio (SocketIO) is connected. If not --> reconnect
  while (!volumio.getConnected())
  {
    USE_SERIAL.print("loop:connecting volumio");
    volumio.connect(volumio_ip, volumio_port);

    if (!volumio.getConnected())
      delay(100);
    else
    {
      volumio.getState();
      volumio.getUiSettings();
    }
  }

  // volumio pushed Updates
  volumio.process();
  switch (volumio.getPushType())
  {
    case Volumio::pushState: //Volumio pushes status update
      USE_SERIAL.print("Volumio: pushState");
      volumio.readState();
      showMusic();
      encoder.setPosition(volumio.State.volume);
      break;
    case Volumio::pushNone:
      break;
    default:
      USE_SERIAL.print("pushState unknown: ");
      USE_SERIAL.println(volumio.getPushType());
  }
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER && (volumio.State.status == "pause" || volumio.State.status == "stop") )
  {
    USE_SERIAL.println(volumio.State.status);
    USE_SERIAL.println("Wakeup but still on pause, stop mode. Go to sleep again."); 
    goToDeepSleep();
    return;
    //gotoSleep=true;
  }else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 && (volumio.State.status == "pause" || volumio.State.status == "stop"))
  {
    USE_SERIAL.println("Wakeup by ESP_SLEEP_WAKEUP_EXT0 (push button). Start to play."); 
    wakeup_reason = ESP_SLEEP_WAKEUP_ALL;
    volumio.play();
  }

  if (volumio.State.service == "mpd")
  {
    showSeek(volumio.State.seek);
  }

  // rotary encoder - volume up/down
  encoder.tick();
  int newRotaryPos = encoder.getPosition();
  int rotaryDirection = (int)encoder.getDirection();
  if (rotaryPos != newRotaryPos && rotaryDirection != 0) {
    
    USE_SERIAL.print("Rotary Direction: ");
    USE_SERIAL.println(rotaryDirection);

    USE_SERIAL.print("Rotary POS: ");
    USE_SERIAL.println(rotaryPos);

    USE_SERIAL.print("New Rotary POS: ");
    USE_SERIAL.println(newRotaryPos);

    if (rotaryDirection == 1 && newRotaryPos < 100)
    {
      USE_SERIAL.println("Add + 10 to VOL");
      newRotaryPos=newRotaryPos+9;
    }else if (rotaryDirection == -1 && newRotaryPos > 0)
    {
      USE_SERIAL.println("Add - 10 to VOL");
      newRotaryPos=newRotaryPos-9;
    }

    if (newRotaryPos > 100)
    {
      newRotaryPos=100;
    }
    if (newRotaryPos < 0)
    {
      newRotaryPos=0;
    }
    encoder.setPosition(newRotaryPos);

    Serial.print("loop Volume pos:");
    Serial.println(newRotaryPos);
    rotaryPos = newRotaryPos;
    controlVolume(newRotaryPos);

  }else if (rotaryPos == newRotaryPos && now < lastsendvolume + 2000) {
    showBigVolume();
  }else{
    showMusic();
  }

  if (volumio.State.status != "play")
  {
    sleepCount++;
    if (sleepCount >= (5*60*10)) { // 5 min = 5min*60s*10ms
      USE_SERIAL.println("No music playing for long time. Go to sleep.");
      goToDeepSleep();
    }
  }else{
    sleepCount = 0;
  }

  if((wifiMulti.run() == WL_CONNECTED)) {
    showWifi();  
  }

  // switch button state
  switch (resultButton) {
    case STATE_SHORT:
      volumio.toggle();
      USE_SERIAL.println("Handle short button state");
      resultButton=STATE_UNPRESSED;
      break;
    case STATE_LONG:
      volumio.stop();
      gotoSleep = true;
      USE_SERIAL.println("Handle long button state");
      resultButton=STATE_UNPRESSED;
      break;
  }

  showBattery();
  u8g2.sendBuffer();
  i++;
  delay(100);
}

void connectWifi()
{
  showSetup();
  wifiMulti.addAP(ssid, password);
  while(wifiMulti.run() != WL_CONNECTED) {
      USE_SERIAL.print(".");
      wifi_counter++;
      if(wifi_counter>=60){ //30 seconds timeout - reset board
        ESP.restart();
      }
      delay(500);
  }

  USE_SERIAL.println("Connected to Wifi");
}

void goToDeepSleep()
{
  USE_SERIAL.println("Going to sleep...");

  u8g2.clearDisplay();
  u8g2.clearBuffer();
  
  u8g2.setFont(u8g2_font_profont12_tf);
  u8g2.drawXBM(11, 9, 105, 13, logo_volumio_big_bits);
  u8g2.drawStr(22,40,"Press to play!");
  u8g2.sendBuffer();

  // dim contrast for display to 10%
  u8g2_t *u8g2_ptr;
  u8g2_ptr = u8g2.getU8g2();
  u8g2_SetContrast(u8g2_ptr, 10);
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();

  adc_power_off();
  esp_wifi_stop();
  esp_bt_controller_disable();

  // Configure the timer to wake us up!
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME * 60L * 1000000L);

  // configure wake up on press play button
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_27,LOW);
 
  // Go to sleep! 
  esp_deep_sleep_start();
}

void shutDown()
{
  USE_SERIAL.println("Shutdown...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();

  adc_power_off();
  esp_wifi_stop();
  esp_bt_controller_disable();

  u8g2.clearDisplay();
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_profont12_tf);
  u8g2.drawStr(5,30,"Battery is empty. Shutdown!");
  u8g2.sendBuffer();

  delay(5000);

  u8g2.clearDisplay();
  u8g2.clearBuffer();
  u8g2.sendBuffer();

  // configure wake up on press play button
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, LOW);
    
  // Go to sleep! 
  esp_deep_sleep_start();
}

void interpretIR(unsigned int &ir_receiver_result)
{

  switch (ir_receiver_result) {
    case 4230337484:
    case 3382655069:
    case 2610347888:
    case 2275880208:
    case 1069840206:
      lastIRCode=ir_receiver_result;
      showIRC();
      volumio.toggle();
      hideIRC();
      break;
    case 3416939178:
    case 2011291728:
    case 125705733:
    case 3310716297:
      lastIRCode=ir_receiver_result;
      showIRC();
      if(volumio.State.service == "webradio" )
      {
        USE_SERIAL.println("Skip cmd next an prev in webradio mode");
        break;
      }
      volumio.next();
      hideIRC();
      break;
    case 2645495272:
    case 2011238480:
    case 1332025175:
    case 125702405:
      lastIRCode=ir_receiver_result;
      showIRC();
      if(volumio.State.service == "webradio" )
      {
        USE_SERIAL.println("Skip cmd next an prev in webradio mode");
        break;
      }
      volumio.prev();
      hideIRC();
      break;
    case 2011287632:
    case 125705477:
    case 4079337264:
      lastIRCode=ir_receiver_result;
      showIRC();
      controlVolume(volumio.State.volume+10);
      hideIRC();
      break;
    case 1778320340:
    case 517546592:
    case 2070423861:
    case 615415119:
    case 2011279440:
      lastIRCode=ir_receiver_result;
      showIRC();
      controlVolume(volumio.State.volume-10);
      hideIRC();
      break;
    case 4294967295:
      // repeat button      
      if(lastIRCode){
        interpretIR(lastIRCode);
      }

      break;
    default:
      USE_SERIAL.print("No cmd for ir code:");
      USE_SERIAL.println(ir_receiver_result);
      return;
  }
}

ICACHE_RAM_ATTR void checkPosition()
{
  encoder.tick(); // just call tick() to check the state.
}

void checkButton() {
  /*
  * This function implements software debouncing for a two-state button.
  * It responds to a short press and a long press and identifies between
  * the two states. Your sketch can continue processing while the button
  * function is driven by pin changes.
  */
  const unsigned long LONG_DELTA = 1000ul;          // hold seconds for a long press
  const unsigned long DEBOUNCE_DELTA = 30ul;        // debounce time
  static int lastButtonStatus = HIGH;               // HIGH indicates the button is NOT pressed
  int buttonStatus;                                     // button atate Pressed/LOW; Open/HIGH
  static unsigned long longTime = 0ul, shortTime = 0ul; // future times to determine is button has been poressed a short or long time
  boolean Released = true, Transition = false;          // various button states
  boolean timeoutShort = false, timeoutLong = false;    // flags for the state of the presses

  buttonStatus = digitalRead(PIN_Encoder_SW);               // read the button state on the pin "BUTTON_PIN"
  timeoutShort = (millis() > shortTime);                // calculate the current time states for the button presses
  timeoutLong = (millis() > longTime);

  if (buttonStatus != lastButtonStatus) {             // reset the timeouts if the button state changed
      shortTime = millis() + DEBOUNCE_DELTA;
      longTime = millis() + LONG_DELTA;
  }

  Transition = (buttonStatus != lastButtonStatus);    // has the button changed state
  Released = (Transition && (buttonStatus == HIGH));  // for input pullup circuit

  lastButtonStatus = buttonStatus;                    // save the button status

  if ( ! Transition) {                                //without a transition, there's no change in input
  // if there has not been a transition, don't change the previous result
       resultButton =  STATE_UNPRESSED | resultButton;
       return;
  }

  if (timeoutLong && Released) {                      // long timeout has occurred and the button was just released
       resultButton = STATE_LONG | resultButton;      // ensure the button result reflects a long press
  } else if (timeoutShort && Released) {              // short timeout has occurred (and not long timeout) and button was just released
      resultButton = STATE_SHORT | resultButton;      // ensure the button result reflects a short press
  } else {                                            // else there is no change in status, return the normal state
      resultButton = STATE_UNPRESSED | resultButton;     // with no change in status, ensure no change in button status
  }
}

void showSetup()
{
    u8g2.clearDisplay();
    u8g2.drawXBM(11, 9, 105, 13, logo_volumio_big_bits);

    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.drawStr(10,40,"Connecting Volumio");
    
    u8g2.sendBuffer();
}

void showMusic()
{
  u8g2.clearBuffer();
  showPlayer();
  showVolume();
  if (volumio.State.service == "mpd")
  {
    showTitle();
    showQuality();
    showSeek(volumio.State.seek);
  }else if(volumio.State.service == "webradio")
  {
    showWebradioTitle();
    if (volumio.State.bitrate)
    {
      showBitrate();
    }
  }
}

double getBatteryVoltage(){
  int readValue = analogRead(BATTERY_PIN);
  double volt; 
  double currentVolt;
  currentVolt = readValue * 1.73 / 1000;
  batterySignalMedian.add(currentVolt);
  volt = batterySignalMedian.getMedian();

  return volt;
}

double getBatteryChargeLevel(double volt){
  /*
  double _vs[11];
  _vs[0] = 3.400; 
  _vs[1] = 3.575;
  _vs[2] = 3.600;
  _vs[3] = 3.730;
  _vs[4] = 3.780;
  _vs[5] = 3.830;
  _vs[6] = 3.900;
  _vs[7] = 3.970;
  _vs[8] = 4.030;
  _vs[9] = 4.090;
  _vs[10] = 4.130;
  */

  double _vs[11];
  _vs[0] = 3.400; 
  _vs[1] = 3.575;
  _vs[2] = 3.600;
  _vs[3] = 3.730;
  _vs[4] = 3.780;
  _vs[5] = 3.800;
  _vs[6] = 3.820;
  _vs[7] = 3.840;
  _vs[8] = 3.870;
  _vs[9] = 3.920;
  _vs[10] = 3.950;

  for(int i = 10; i >= 0; i--){
    if (volt >= _vs[i] )
    {
      return i;
    }
  }

  return 0;
}

void showBattery()
{
  double volt = getBatteryVoltage(); 
  int chargeLevel = getBatteryChargeLevel(volt);

  // shutdown to save battery
  if (volt <= 3.2)
  {
    shutDown();
  }

  int xpos = 2;
  int ypos = 9;

  // draw battery icon
  u8g2.setDrawColor(1);
  u8g2.drawFrame(xpos,ypos-8,12,8);
  u8g2.drawFrame(xpos+12,ypos-5,2,2);

  // calculate battery_width
  int battery_width = chargeLevel;
  u8g2.drawBox(xpos+1,ypos-7,battery_width,6);

  // Debug battery voltage
  char str_volt[5];
  dtostrf(volt, 4, 2, str_volt);

  // draw black box over exisiting text
  u8g2.setDrawColor(0);
  u8g2.drawRBox(112,17+26+10,30,12,0);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_p01type_tf);
  u8g2.drawStr(114,27+26+10,str_volt);
}

void showSeek(int v_seek)
{
  unsigned long allSeconds=(v_seek)/1000;
  //int runHours= allSeconds/3600;
  int secsRemaining=allSeconds%3600;
  int runMinutes=secsRemaining/60;
  int runSeconds=secsRemaining%60;
  
  char seek_time[9];
  sprintf(seek_time,"%02d:%02d",runMinutes,runSeconds);

  // draw black box over exisiting text
  u8g2.setDrawColor(0);
  u8g2.drawRBox(55,0,30,9,0);

  u8g2.setFont(u8g2_font_glasstown_nbp_tf);
  u8g2.setDrawColor(1);
  u8g2.drawStr(55,9,seek_time);
}

void showQuality()
{
  u8g2.setDrawColor(1);

  if(volumio.State.trackType == "mp3")
  {
    u8g2.drawRBox(85,0,18,9,0);
    u8g2.setFont(u8g2_font_p01type_tf);
    u8g2.setDrawColor(0);
    u8g2.drawStr(87,7,"MP3");
  }else if(volumio.State.trackType == "flac"){
    u8g2.drawRBox(85,0,23,9,0);
    u8g2.setFont(u8g2_font_p01type_tf);
    u8g2.setDrawColor(0);
    u8g2.drawStr(87,7,"FLAC");
  }
}

void showBitrate()
{
  char bitrate[4];
  volumio.State.bitrate.toCharArray(bitrate,4);
  bitrate[3] = '\0';

  u8g2.setDrawColor(1);
  u8g2.drawRBox(85,0,16,9,0);
  u8g2.setFont(u8g2_font_p01type_tf);
  u8g2.setDrawColor(0);
  u8g2.drawStr(89,7,bitrate);
}

void showVolume()
{
    u8g2.setFont(u8g2_font_open_iconic_play_1x_t);
    u8g2.setDrawColor(1);

    if(volumio.State.volume <= 1){
      u8g2.drawGlyph(21,9,81);
      u8g2.setFont(u8g2_font_p01type_tf);
      u8g2.drawStr(27,7,"X");

    }else if(volumio.State.volume < 41){
      u8g2.drawGlyph(21,9,81);
    }else if (volumio.State.volume > 90 ){
      u8g2.drawGlyph(21,9,79);
    }else if(volumio.State.volume > 40){
      u8g2.drawGlyph(21,9,80);
    }
}

void showWifi()
{
  long messRSSI = WiFi.RSSI();

  wifiSignalMedian.add(messRSSI);
  long RSSI = wifiSignalMedian.getMedian();

  // draw wifi symbol
  u8g2.setFont(u8g2_font_open_iconic_other_1x_t);
  u8g2.setDrawColor(1);
  u8g2.drawGlyph(120,9,70);

  // hide wifi symbol in dependence of wifi signal
  u8g2.setDrawColor(0);
  if (RSSI >= -55) { 
    // 4 bars
  } else if (RSSI < -55 && RSSI >= -70) {
    // 3 bars
    u8g2.drawBox(126,1,2,8);
  } else if (RSSI < -70 && RSSI >= -78) {
    // 2 bars
    u8g2.drawBox(124,1,4,8);
  } else if (RSSI < -78 && RSSI >= -82) {
    // 1 bar
    u8g2.drawBox(122,1,6,8);
  } else {
    // 0 bar
    u8g2.drawBox(120,1,8,8);
  }
  
}

void showIRC()
{
    u8g2.setFont(u8g2_font_open_iconic_www_1x_t);
    u8g2.setDrawColor(1);   
    u8g2.drawGlyph(108,9,81);
}

void hideIRC()
{
    u8g2.setFont(u8g2_font_open_iconic_www_1x_t);
    u8g2.setDrawColor(0);   
    u8g2.drawGlyph(108,9,81);
}

void showPlayer()
{
    u8g2_uint_t midOriginX = 64;

    u8g2.setDrawColor(1);
    u8g2.drawRBox(midOriginX-28,0,10,10,2);
    u8g2.setDrawColor(0);
    u8g2.setFont( u8g2_font_open_iconic_play_1x_t);

    if(volumio.State.status == "play")
    {
      u8g2.drawGlyph(midOriginX-27,9,69);
    }

    if(volumio.State.status == "stop")
    {
      u8g2.drawGlyph(midOriginX-27,9,75);
    }
    
    if(volumio.State.status == "pause")
    {
      u8g2.drawGlyph(midOriginX-27,9,68);
    }  
}

void showWebradioTitle()
{
    u8g2.setFont(u8g2_font_glasstown_nbp_tf);
    
    u8g2.setDrawColor(1);
    u8g2.drawRBox(2,17,30,12,0);
    u8g2.setDrawColor(0);
    u8g2.drawStr(4,27,"Radio");
    u8g2.setDrawColor(1);
    
    if (volumio.State.artist != "null")
    {
      // shorten the artist = radiosstation
      char artist[35];
      
      volumio.State.artist.toCharArray(artist,34);
      artist[34] = '\0';
      u8g2.drawUTF8(34,27,artist);
      u8g2.setDrawColor(1);

      // check if title has '-'
      int x = volumio.State.title.indexOf('-');
      if( x != -1)
      {
        String v_artist="";
        String v_title="";
        // Split title at '-'
        v_artist = volumio.State.title.substring(0,(x-1));
        v_title = volumio.State.title.substring(x+2,volumio.State.title.length());

        u8g2.drawRBox(2,17+13,30,12,0);
        u8g2.setDrawColor(0);
        u8g2.drawStr(4,27+13,"Artist");
        u8g2.setDrawColor(1);
        // shorten the title
        char artist[35];
        v_artist.toCharArray(artist,34);
        artist[34] = '\0';
        u8g2.drawUTF8(34,27+13,artist);

        u8g2.setDrawColor(1);
        u8g2.drawRBox(2,17+26,30,12,0);
        u8g2.setDrawColor(0);
        u8g2.drawStr(4,27+26,"Title");
        u8g2.setDrawColor(1);
        // shorten the title
        char title[35];
        v_title.toCharArray(title,34);
        title[34] = '\0';
        u8g2.drawUTF8(34,27+26,title);

      }else{
        u8g2.drawRBox(2,17+13,30,12,0);
        u8g2.setDrawColor(0);
        u8g2.drawStr(4,27+13,"Title");
        u8g2.setDrawColor(1);
        // shorten the title
        char title[35];
        volumio.State.title.toCharArray(title,34);
        title[34] = '\0';
        u8g2.drawUTF8(34,27+13,title);
      }
      
    }else{  // radio is pause or stop
      // shorten the title
      char title[35];
      volumio.State.title.toCharArray(title,34);
      title[34] = '\0';
      u8g2.drawUTF8(34,27,title);
      u8g2.setDrawColor(1);
    }
    
}

void showTitle()
{
    u8g2.setFont(u8g2_font_glasstown_nbp_tf);
    
    u8g2.setDrawColor(1);
    u8g2.drawRBox(2,17,30,12,0);
    u8g2.setDrawColor(0);
    u8g2.drawStr(4,27,"Artist");
    u8g2.setDrawColor(1);
    // shorten the artist
    char artist[35];
    //strncpy(artist, v_artist, 34);
    volumio.State.artist.toCharArray(artist,34);
    artist[34] = '\0';
    u8g2.drawUTF8(34,27,artist);

    u8g2.setDrawColor(1);
    u8g2.drawRBox(2,17+13,30,12,0);
    u8g2.setDrawColor(0);
    u8g2.drawStr(4,27+13,"Album");
    u8g2.setDrawColor(1);
    // shorten the album
    char album[35];
    //strncpy(album, v_album, 34);
    volumio.State.album.toCharArray(album,34);
    album[34] = '\0';
    u8g2.drawUTF8(34,27+13,album);

    u8g2.setDrawColor(1);
    u8g2.drawRBox(2,17+26,30,12,0);
    u8g2.setDrawColor(0);
    u8g2.drawStr(4,27+26,"Title");
    u8g2.setDrawColor(1);
    // shorten the title
    char title[35];
    volumio.State.title.toCharArray(title,34);
    title[34] = '\0';
    u8g2.drawUTF8(34,27+26,title);
}

void controlVolume(int level)
{
  USE_SERIAL.print("controlVolume - level: ");
  USE_SERIAL.println(level); 

  unsigned long now = millis();
  showBigVolume();

  if (level <= 100 && level >= 0 )
  {
    if (volumio.State.volume != level)
    {
      USE_SERIAL.println("Try to change Volume");
      if (now > lastsendvolume + 300)
      {
        USE_SERIAL.print("change Volume to: ");
        USE_SERIAL.print(level);
        volumio.volume(level);
        lastsendvolume = now;
      }
    }
  }
  
  showBigVolume();
}

void showBigVolume()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso42_tr);

  u8g2.setDrawColor(1);
  char vol[4];
  sprintf(vol, "%02d", volumio.State.volume);
  vol[3] = '\0';
  u8g2.drawStr(30,60,vol);
  u8g2.sendBuffer();
}

void showDebugMessage(const char* message)
{
  u8g2.clearDisplay();
  u8g2.setFont(u8g2_font_profont12_tf);
  //FIXME: add line break
  u8g2.drawUTF8(10,30, message);
  u8g2.sendBuffer();
}