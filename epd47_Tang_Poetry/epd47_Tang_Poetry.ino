#include "hz7000_24.h"
#include "Poetry300.h"
//memo缓存管理
#include "memo_historyManager.h"


/*
  功能： 每12小时随机显示唐诗300首中的一首诗
  开发板：TTGO-T-WATCH
  分区: Default
  PSRAM: Enabled

  编译大小: 3.2M (注： 因为诗词原因，用了7000个汉字的字库文件占用空间很大)

  汉字显示： 18字* 9行= 162 字      (每汉字*3= 486字节)
  唤醒后工作时间: 4秒
  休眠时间 12小时（每天刷新2次）

  说明：
    3500字库可以显示36点阵字体，但显示诗时，有些字无法显示
    7000显示诗时，汉字无法显示机率变小，但墨水屏驱动最大只能支持24点阵字体，不支持36点阵字体
    如果遇到字库没有的汉字，可能会跳过本字或其它异常
*/

//离rst最远那个 开机时如果按住清屏，且不显示内容
#define BUTTON_BOOT_PRESS  39
bool pin_link = false;

memo_historyManager* objmemo_historyManager;

//墨水屏缓存区
uint8_t *framebuffer;

uint32_t stoptime, starttime;

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
uint32_t TIME_TO_SLEEP = 3600 * 12; //下次唤醒间隔时间(秒）

hw_timer_t *timer = NULL;
int dog_timer = 1; //定时狗的触发分钟数,开机指定时间没有响应，自动重启

void IRAM_ATTR resetModule() {
  ets_printf("resetModule reboot\n");
  delay(100);
  //esp_restart_noos(); 旧api
  esp_restart();
}

//判断字串中某字符出现次数
int word_dot_count(String str, String dot_str)
{
  int p1 = 0;
  int cnt = 0;
  while (p1 > -1)
  {
    p1 = str.indexOf(dot_str);
    if (p1 > -1)
    {
      //Serial.println("p1=" + String(p1));
      str.setCharAt(p1, ' ');
      cnt++;
    }
  }
  return cnt;
}


bool check_pin()
{
  pinMode(BUTTON_BOOT_PRESS, INPUT);

  delay(500);
  int pin_link = digitalRead(BUTTON_BOOT_PRESS) ;

  if (pin_link == 0)
    return true;
  else
    return false;
}

//GetCharwidth函数本来应放在 memo_historyManager类内部
//但因为引用了 msyh24海量字库变量，会造成编译失败,所以使用了一些技巧
//将函数当指针供类memo_historyManager 使用
//计算墨水屏显示的单个字的长度
int GetCharwidth(String ch)
{
  //修正，空格计算的的宽度为0, 强制36 字体不一样可能需要修改！
  if (ch == " ") return 28;

  char buf[50];
  int x1 = 0, y1 = 0, w = 0, h = 0;
  int tmp_cur_x = 0;
  int tmp_cur_y = 0;
  FontProperties properties;
  get_text_bounds((GFXfont *)&msyh, (char *) ch.c_str(), &tmp_cur_x, &tmp_cur_y, &x1, &y1, &w, &h, &properties);
  //sprintf(buf, "x1=%d,y1=%d,w=%d,h=%d", x1, y1, w, h);
  //Serial.println("ch="+ ch + ","+ buf);

  //负数说明没找到这个字,会不显示出来
  if (w <= 0)
    w = 0;
  return (w);
}

//文字显示
void Show_hz(String rec_text, bool loadbutton)
{

  //更正汉字符号显示的bug
  rec_text.replace("，", ",");
  rec_text.replace("。", ".");
  rec_text.replace("？", "?");
  rec_text.replace("！", "!");

  Serial.println("Show_hz 长度:" + String(rec_text.length()));


  //16*9*3 满屏最多 144汉字 384字节
  rec_text = rec_text.substring(0, 432);

  //Serial.println("begin Showhz:" + rec_text);

  epd_poweron();
  //uint32_t t1 = millis();
  //全局刷
  epd_clear();
  //局刷,一样闪屏
  //epd_clear_area(screen_area);
  //epd_full_screen()

  //此句不要缺少，否则显示会乱码
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  //特殊标志符
  if (rec_text != "[clear]")
  {
    //uint32_t t2 = millis();
    //printf("EPD clear took %dms.\n", t2 - t1);
    int cursor_x = 10;
    int cursor_y = 57;  //36字体 80 24字体

    //多行文本换行显示算法。
    if (!loadbutton)
      objmemo_historyManager->multi_append_txt_list(rec_text);

    String now_string = "";
    int i;
    //write_string 能根据手工加的 "\n"换行，但不能自由控制行距，此处我自行控制了.
    for ( i = 0; i < objmemo_historyManager->memolist.size() ; i++)
    {
      now_string = objmemo_historyManager->memolist.get(i);
      //Serial.println("Show_hz line:" + String((now_index + i) % TXT_LIST_NUM) + " " + now_string);

      if (now_string.length() > 0)
      {
        //加">"字符，规避epd47的bug,当所有字库不在字库时，esp32会异常重启
        // “Guru Meditation Error: Core 1 panic'ed (LoadProhibited). Exception was unhandled."
        now_string = ">" + now_string;
        //墨水屏writeln不支持自动换行
        //delay(200);
        //一定要用framebuffer参数，否则当最后一行数据过长时，会导致代码在此句阻塞，无法休眠，原因不明！

        writeln((GFXfont *)&msyh, (char *)now_string.c_str(), &cursor_x, &cursor_y, framebuffer);
        //writeln调用后，cursor_x会改变，需要重新赋值
        cursor_x = 10;
        cursor_y = cursor_y + 57;  //字体36 加85  字体24 加 57
      }
    }
  }
  //前面不要用writeln，有一定机率阻塞，无法休眠
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);

  //delay(500);
  epd_poweroff();

  //清空显示
  objmemo_historyManager->memolist.clear();
  //Serial.println("end Showhz:" + rec_text );
}

void goto_sleep()
{
  Serial.println("goto sleep");
  stoptime = millis() / 1000;
  Serial.println("wake 用时:" + String(stoptime - starttime) + "秒");
  Serial.println("go sleep,wake after " + String(TIME_TO_SLEEP)  + "秒");
  Serial.flush();

  //It will turn off the power of the entire
  // POWER_EN control and also turn off the blue LED light
  epd_poweroff_all();

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // ESP进入deepSleep状态
  //休眠后，GPIP的高，低状态将失效，无法用GPIO控制开关
  esp_deep_sleep_start();
}



void show_poetry()
{
  String show_txt = "";

  int line_cnt = 100;

  //判断开机时有按键，清屏不显示内容
  if (not pin_link)
  {

    String Poetry_title ;
    String Poetry_contents  ;
    String Poetry_author  ;
    String Poetry_type ;

    //8行以上地不显示,重新取数
    while (line_cnt >= 8)
    {
      int Poetry_index = random(PoetryCount / 5);

      Serial.println("随机获取第" + String(Poetry_index+1)+ "首诗");

      Poetry_title = PoetryArray[Poetry_index * 5 + 4] ;
      Poetry_contents = PoetryArray[Poetry_index * 5 + 1] ;
      Poetry_author = PoetryArray[Poetry_index * 5 + 3] ;
      Poetry_type = PoetryArray[Poetry_index * 5 + 2] ;

      Poetry_title.replace("\"title\": ", "");
      Poetry_contents.replace("\"contents\": ", "");
      Poetry_author.replace("\"author\": ", "");
      Poetry_type.replace("\"type\": ", "");

      Poetry_title.replace("\"", "");
      Poetry_contents.replace("\"", "");
      Poetry_author.replace("\"", "");
      Poetry_type.replace("\"", "");

      Poetry_title.trim();
      Poetry_contents.trim();
      Poetry_author.trim();
      Poetry_type.trim();

      line_cnt = word_dot_count(Poetry_contents, "\n");
      Serial.println("本诗行数=" + String(line_cnt+1));

      if (line_cnt >= 8)
        Serial.println("本诗行数超过8行，显示屏无法展示完全，跳过本诗...");
    }

    //如果只有4行，拆成最多8行显示
    if (line_cnt <= 3)
    {
      Poetry_contents.replace("\n", "");
      Poetry_contents.replace("，", "，\n");
      Poetry_contents.replace("。", "。\n");
      Poetry_contents.replace("？", "？\n");
      Poetry_contents.replace("！", "！\n");
    }

    Serial.println(String(Poetry_title ) + " ("+String(Poetry_author)+")");
    //3字节/汉字 24汉字= 72字节
    Serial.println(String(Poetry_contents) + "诗正文字节长度=" + String(Poetry_contents.length())  );
    show_txt = Poetry_title + " (" + Poetry_author +  ")\n" + Poetry_contents;
    Serial.println("调用 show_txt()函数, 字节总长度:" +  String(show_txt.length())  );
  }

  Show_hz(show_txt, false);
}

void setup() {
  starttime = millis() / 1000;
  Serial.begin(115200);

  pin_link = check_pin();

  if (pin_link)
    Serial.println("button pressed!");


  Serial.println("set timer dog:" + String(dog_timer) + "分钟");
  int wdtTimeout = dog_timer * 60 * 1000; //设n分钟 watchdog

  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000 , false); //set time in us
  timerAlarmEnable(timer);                          //enable interrupt
  timerWrite(timer, 0); //reset timer (feed watchdog)


  //如果启动后不调用此函数，有可能电流一直保持在在60ma，起不到节能效果
  //此步骤不适合在唤醒后没有显示需求时优化掉
  epd_init();
  // framebuffer = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) {
    Serial.println("alloc memory failed !!!");
    delay(1000);
    while (1);
  }
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  objmemo_historyManager = new memo_historyManager();
  objmemo_historyManager->GetCharwidth = GetCharwidth;

  Serial.println("start");

  show_poetry();

  //进入休眠
  goto_sleep();
}


void loop() {

}
