/***************************************************
 *   UV Meter.
 *
 * Connect:
 * * ML8511 to A4 input
 * * ADAFRUIT ADA1774 2.8 TFT Display With Resistive Touchscreen
*/

/***************************************************
   Includes code from example from DfRobot:

   By Phoebe phoebe.wang@dfrobot.com>

   GNU Lesser General Public License.
   See <http://www.gnu.org/licenses/> for details.
   All above must be included in any redistribution
 ****************************************************/

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "Adafruit_STMPE610.h"

// For the Adafruit shield, these are the default.
#define TFT_DC 9
#define TFT_CS 10

// Compile with additional serial output.
#define DEBUG 1

// Basic colors
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

// Give names to text sizes
#define TEXT_SMALL 1
#define TEXT_MEDIUM 2
#define TEXT_LARGE 3

// Define some basic padding parameters.  Gives different padding of background colour around text box.
#define PAD_NONE 0
#define PAD_SMALL 2
#define PAD_MEDIEM 5
#define PAD_LARGE 10

// 2 types of UV source with different UV targets.  Could add mercury vapour in future.
enum globe_t {TUBE, CF};

// Use a analog pin outside range of tft / keypad range.
int ReadUVintensityPin = A4; //Output from the sensor.  Avoid the touch screen pins A0 -> A3.

// Current maximum intensity seen.
float max_intensity = 0.0;

// volatage that represents 0 Mw/cm2 UV.  Moves a bit so allow onloine recalibration.
float zero_voltage = 0.99;

// Set warning levels for CF and TUBES. This is based on limited data at this stage.
#define TUBE_LOW_THRESH 0.8
#define TUBE_HI_THRESH 1.4
#define CF_LOW_THRESH 2.0
#define CF_HI_THRESH 3.5

// Color indications of level
#define COLOR_LOW 0xF800 // RED
#define COLOR_MED 0xFFE0 // YELLOW
#define COLOR_HI 0x07E0 // GREEN

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// Text box object that allows contol of basic display parameters.
class Box
{
  protected:
    int x, y, width, height;  // box size and posn.
    Adafruit_ILI9341* tft;
    String text = "";
    unsigned int bk_color, text_color;
    unsigned short text_size, padding;  // text size in units used by graphics library.  padding around text for text box.
    bool changed = true; // track if display has changed to avoid repainting unchanged objects causing flickering.

  public:

    Box(Adafruit_ILI9341* _tft, int _x, int _y, int _width, int _height, String _text,
        unsigned _bk_color = WHITE, unsigned int _text_color = BLACK, unsigned short _text_size = 2, unsigned short _padding = 10):
        tft(_tft), x(_x), y(_y), height(_height), width(_width), text(_text), bk_color(_bk_color), text_color(_text_color), text_size(_text_size), padding(_padding)
    {}

    // Repaint object if something has changed.
    void display(void)
    {
      // Avoid flickering by repainting if something changes.
      if (changed) {
        tft->fillRect(x, y, width, height, bk_color);
        tft->setCursor(x + padding, y + padding);
        tft->setTextColor(text_color);
        tft->setTextSize(text_size);

        tft->println(text);
        changed = false;
      }
    }

    // Set methods
    void setText(String t)
    {
      if (strcmp(text.c_str(), t.c_str())) {
        text = t;
        changed = true;     // set changed flag to trigger repaint.
      }
    }

    void setText(float f)
    {
      String str = String(f);
      setText(str);
    }

    virtual unsigned int getBgColor(void)
    {
      return bk_color;
    }

    unsigned int setBgColor(unsigned int col)
    {
      if (col != bk_color) {
        changed = true;   // set changed flag to trigger repaint.
        bk_color = col;
      }

      return bk_color;
    }

    unsigned int setTextColor(unsigned int col)
    {
      if (col != text_color) {
        changed = true; // set changed flag to trigger repaint.
        text_color = col;
      }
      return text_color;
    }


};


// Button class that allows objects to be clicked.  
class Button : public Box
{
  private:
    // Provide 2 states: selected and unselected, each with different display color.
    unsigned int unselect_color;
    unsigned int select_color;
    bool selected = false;    

  public:
    // unselect color is default bk_color.  Add additional selected_color.
    Button(Adafruit_ILI9341* _tft, int _x, int _y, int _width, int _height, String _text, unsigned _bk_color = WHITE,
           unsigned int _text_color = BLACK, unsigned short _text_size = 2, unsigned int _select_color = GREEN):
      Box(_tft, _x, _y, _width, _height, _text, _bk_color, _text_color, _text_size), select_color(_select_color), unselect_color(_bk_color) {}

    // Set selected state.
    void setSelected(bool s)
    {
      selected = s;
      setBgColor((selected) ? select_color : unselect_color);
    }

    bool isSelected()
    {
      return selected;
    }

    // Check a click position against the object position.
    bool isIn(TS_Point p)
    {
      bool in =  ((p.x >= x && p.x <= (x + width)) &&
                  (p.y >= y && p.y <= y + height));


      if (DEBUG && in) {
        Serial.print("Is in "); Serial.println(text);
      }
      return in;
    }
};

// Define buttons
Button btnReset = Button(&tft, 20, 250, 90, 40, "Reset", WHITE, BLACK, TEXT_MEDIUM);
Button btnZero = Button(&tft, 130, 250, 90, 40, "CAL", WHITE, BLACK, TEXT_MEDIUM);
Button btnTube = Button(&tft, 20, 200, 90, 40, "Tube", WHITE, BLACK, TEXT_MEDIUM, GREEN);
Button btnCompact = Button(&tft, 130, 200, 90, 40, "CF", WHITE, BLACK, TEXT_MEDIUM, GREEN);

// Define other objects
Box boxTitle = Box(&tft, 20, 10, 200, 40, " UV Meter", WHITE, BLUE, TEXT_LARGE);
Box boxMaxTitle = Box(&tft, 60, 90, 30, 10, "MAX", BLACK, WHITE, TEXT_SMALL, PAD_NONE);
Box boxCurrentTitle = Box(&tft, 160, 90, 40, 10, "CURRENT", BLACK, WHITE, TEXT_SMALL, PAD_NONE);
Box boxMaxInt = Box(&tft, 25, 110, 80, 30, "0.0", BLACK, WHITE, TEXT_LARGE, PAD_NONE);
Box boxMaxIntUnits = Box(&tft, 105, 110, 20, 20, "mW/cm2", BLACK, WHITE, TEXT_SMALL, PAD_NONE);
Box boxCurrentInt = Box(&tft, 150, 110, 50, 20, "0.00", BLACK, WHITE, TEXT_MEDIUM, PAD_NONE);
Box boxCurrentIntUnits1 = Box(&tft, 205, 110, 40, 10, "mW/", BLACK, WHITE, TEXT_SMALL, PAD_NONE);
Box boxCurrentIntUnits2 = Box(&tft, 205, 120, 40, 10, "cm2", BLACK, WHITE, TEXT_SMALL, PAD_NONE);
Box boxCurrentV = Box(&tft, 150, 140, 50, 20, "1.00", BLACK, WHITE, TEXT_MEDIUM, PAD_NONE);
Box boxCurrentVUnits = Box(&tft, 205, 145, 20, 20, "V", BLACK, WHITE, TEXT_SMALL, PAD_NONE);

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 150
#define TS_MINY 130
#define TS_MAXX 3800
#define TS_MAXY 4000

// The STMPE610 uses hardware SPI on the shield, and #8
#define STMPE_CS 8
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);


void setup()
{
  pinMode(ReadUVintensityPin, INPUT);
  
  Serial.begin(9600); //open serial port, set the baud rate to 9600 bps
  Serial.println("Starting up...");

  tft.begin();

  // read diagnostics (optional but can help debug problems)
  uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDMADCTL);
  Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDPIXFMT);
  Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDIMGFMT);
  Serial.print("Image Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDSELFDIAG);
  Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX);
  Serial.print("Screen Width : "); Serial.println(tft.width());
  Serial.print("Screen Height : "); Serial.println(tft.height());

  if (!ts.begin()) {
    Serial.println("Couldn't start touchscreen controller");
    while (1);
  }
  Serial.println("Touchscreen started");

  tft.setRotation(2);
  tft.fillScreen(ILI9341_BLACK);

  btnTube.setSelected(true);

  display_all();

}

void reset_max()
{
  max_intensity = 0.0;
  Serial.println("Reset Max");
}

void zero(float v)
{
  zero_voltage = v;
  reset_max();
  Serial.println("Zero Voltage");
}

void process_touch(float currentVoltage) {
  // See if there's any  touch data for us
  while (!ts.bufferEmpty()) {

    // Retrieve a point
    TS_Point p = ts.getPoint();
    p.x = map(TS_MAXY - p.x, TS_MINY, TS_MAXY, 0, tft.width());
    p.y = map(TS_MAXX - p.y, TS_MINX, TS_MAXX, 0, tft.height());

    if (DEBUG) {
      Serial.print("Click ");
      Serial.print(p.x);
      Serial.print(" ");
      Serial.println(p.y);
    }

    if (btnZero.isIn(p)) {
      zero(currentVoltage);
      if (DEBUG) {
        Serial.println("Execute Zero");
      }
    }

    if (btnReset.isIn(p)) {
      reset_max();
      if (DEBUG) {
        Serial.println("Execute Reset");
      }
    }

    if (btnTube.isIn(p)) {
      btnTube.setSelected(true);
      btnCompact.setSelected(false);
    }
    if (btnCompact.isIn(p)) {
      btnTube.setSelected(false);
      btnCompact.setSelected(true);
    }

  }
}

void display_all() {
  btnZero.display();
  btnReset.display();
  btnTube.display();
  boxMaxTitle.display();
  boxCurrentTitle.display();
  btnCompact.display();
  boxTitle.display();
  boxMaxInt.display();
  boxMaxIntUnits.display();
  boxCurrentInt.display();
  boxCurrentIntUnits1.display();
  boxCurrentIntUnits2.display();
  boxCurrentV.display();
  boxCurrentVUnits.display();

}

// Determine the color based on the intensity against the defined thresholds.
//
unsigned int getColor(float intensity, globe_t globe) {

  unsigned int color = WHITE;
  if (globe == TUBE) {
    if (intensity < TUBE_LOW_THRESH) {
      color = COLOR_LOW;
    } else if (intensity < TUBE_HI_THRESH) {
      color = COLOR_MED;
    } else {
      color = COLOR_HI;
    }
  } else {
    if (intensity < CF_LOW_THRESH) {
      color = COLOR_LOW;
    } else if (intensity < CF_HI_THRESH) {
      color = COLOR_MED;
    } else {
      color = COLOR_HI;
    }
  }
  return color;
}

// Main Loop
void loop()
{
  int uvLevel = averageAnalogRead(ReadUVintensityPin);

  float outputVoltage = 5.0 * uvLevel / 1024;
  float uvIntensity = mapfloat(outputVoltage, zero_voltage, 2.9, 0.0, 15.0);

  Serial.print("UVAnalogOutput: ");
  Serial.print(uvLevel);

  Serial.print(" OutputVoltage: ");
  Serial.print(outputVoltage);

  Serial.print(" UV Intensity: ");
  Serial.print(uvIntensity);
  Serial.print(" mW/cm^2");

  if (uvIntensity > max_intensity) {
    max_intensity = uvIntensity;
  }

  // Clip at zero to avoid shifting the display with a -ve sign.
  max_intensity = max(max_intensity, 0.0);
  Serial.print(" Max UV Intensity: ");
  Serial.print(max_intensity);
  Serial.print(" mW/cm^2");

  Serial.println();

  globe_t globe = (btnCompact.isSelected()) ? CF : TUBE;

  boxMaxInt.setText(max_intensity);
  boxMaxInt.setTextColor(getColor(max_intensity, globe));

  boxCurrentInt.setText(max(uvIntensity, 0.0));
  boxCurrentInt.setTextColor(getColor(uvIntensity, globe));

  boxCurrentV.setText(outputVoltage);

  process_touch(outputVoltage);

  display_all();
  delay(500);
}

//Takes an average of readings on a given pin
//Returns the average
int averageAnalogRead(int pinToRead)
{
  byte numberOfReadings = 8;
  unsigned int runningValue = 0;

  for (int x = 0 ; x < numberOfReadings ; x++)
    runningValue += analogRead(pinToRead);
  runningValue /= numberOfReadings;

  return (runningValue);

}

//The Arduino Map function but for floats
//From: http://forum.arduino.cc/index.php?topic=3922.0
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

