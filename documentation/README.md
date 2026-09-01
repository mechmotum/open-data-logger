# Documentation
For a more concise manual, refer to the [Quick Start Guide](/documentation/quickstart.md).

This document serves as the main guide on how to work with the datalogger. It is roughly divided into three parts:

+ Usage
+ Software
+ Further development

## Usage
This part covers the steps on how to use the datalogger

### 1. Setup
To set up the logger, a configuration file needs to be present in the "config" folder on its SD card. The format for this file is as follows: [config.txt](/documentation/config.txt).
The name of this file is to be changed to [project].txt if there are multiple configurations on the same SD card.

In this file, only lines without // or # infront of them are read.

The ability to configure via a file mostly serves as a solid foundation for the future development of this project: this method can support a multitude of different sensors and can easily be expanded upon.
Currently, the configuration of the datalogger is quite limited due to the small amount of sensor units available. The most important setting to change is thus if the data is to be written to a new file every run, or appended to a single large csv. This can the done by enabling the singleFile line in the file.


### 2. Connections

The default configuration is currently:
- Port 1: Steer Angle Sensor
- Port 2: Wheel Speed Sensor

In the current state of the datalogger, there is little to no reason to change this configuration.


### 3. Measuring

To start measuring, navigate to the measure tab in the menu and press start measuring. This will create a file called DATAXXX.csv in the data folder

### 4. Analysis

After conducting experiments, one might want to quickly check their data output before further analysis specific to each project. To facilitate this, analyse.py can be used.
Create a folder within the data directory in this repository and place the .csv files of the runs into it. After this, simply run analyse.py and it will display the data either per run or per data entry. On the right side, graphs can be disabled for clarity.

## Technical Details
### Main Modules
+ Controller box
    + User interface
    + Teensy 4.1 Microcontroller
    + ICM20948 IMU
+ Analog Splitter:
    + 10-pin in -> 4x 4-pin out
+ Digital Splitter:
    + 6-pin in -> 4x 6-pin out

While there is a distinction made between digital and analog sensors, the sensors are moreso seperated by their ability to use I2C or not. Digital sensors that cannot use this protocol, like the wheel speed sensor, also need to be connected to the analog splitter.

### Available Sensors
+ Steer angle sensor (*analog*)
    + 10 kOhm potentiometer
+ Wheel speed sensor (*analog*)
    + XX Hall sensor

### Used Components
The following components have been used for this project
| Component                 | Source Link                                                                                                                                                                                     |
|---------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Teensy 4.1                | https://www.tinytronics.nl/nl/development-boards/microcontroller-boards/teensy/teensy-4.1                                                                                                       |
| Sparkfun ICM20948 IMU     | https://www.sparkfun.com/sparkfun-9dof-imu-breakout-icm-20948-qwiic.html                                                                                                                        |
| GX12-6 Aviator Connector  | https://www.tinytronics.nl/nl/kabels-en-connectoren/connectoren/aviation-style/gx12/gx12-6-connector-set                                                                                        |
| GX16-10 Aviator Connector | https://www.tinytronics.nl/nl/kabels-en-connectoren/connectoren/aviation-style/gx16/gx16-10-connector-set                                                                                       |
| 10 kOhm Potentiometer     | https://www.tinytronics.nl/nl/componenten/weerstanden/potmeters/10k%CF%89-potmeter-standaard                                                                                                    |
| KY-024 Hall Sensor        | https://www.currentcomponents.nl/products/hall-sensor?variant=43724849152254&country=NL&currency=EUR&utm_medium=product_sync&utm_source=google&utm_content=sag_organic&utm_campaign=sag_organic |
| 2.23 inch OLED Display    | https://www.tinytronics.nl/nl/displays/oled/2.23-inch-oled-display-128*32-pixels-wit-i2c                                                                                                        |
| Rotary Encoder            | https://www.tinytronics.nl/nl/schakelaars/manuele-schakelaars/rotary-encoders/rotary-encoder-ec11-20mm                                                                                          |

## Wiring
The logger currently has a capacity of 4 analog sensors and 4 digital sensors. The digital sensors are controller via I2C and don't need pin assignment, but the analog pins are assigned as follows:

+ Slot 1 -- Pin A1 / 15
+ Slot 2 -- Pin A2 / 16
+ Slot 3 -- Pin A3 / 17
+ Slot 4 -- Pin A6 / 20

*The jump is due to pin 18 and 19 being used for I2C*

The aviator outputs exiting the controller box have labeled pin numbers and are colour coded inside the housing:

### Analog Main Pinout *(10 pin)*

| Connector Pin | Teensy Pin | Colour     |
|---------------|------------|------------|
| 1             | TBD        | White      |
| 2             | TBD        | Purple     |
| 3             | TBD        | Gray       |
| 4             | 3V3        | Light Blue |
| 5             | GND        | Black      |
| 6             | 5V         | Red        |
| 7             | Slot 4     | Green      |
| 8             | Slot 3     | Yellow     |
| 9             | Slot 2     | Dark Blue  |
| 10            | Slot 1     | Orange     |


### Digital Main Pinout *(6 pin)*

| Connector Pin | Teensy Pin | Colour |
|---------------|------------|--------|
|               |            |        |
|               |            |        |
|               |            |        |
|               |            |        |
|               |            |        |
|               |            |        |

Furthermore, the outputs from both of the splitters are consistent for all four slots. According to the labeling on the respective aviator connectors, the assignment is as follows:

### Analog Splitter Pinout *(4 pin)*
| Connector Pin | Usage  |
|---------------|--------|
| 1             | Data   |
| 2             | Ground |
| 3             | VCC    |
| 4             | TBD    |

The fourth pin is connected to 3V3 for slot 1, as some sensors, like the potentiometers operate at 3V3. The rest of the slots are left vacant at this pin for future applications

### Digital Splitter Pinout *(6 pin)*
The output pins share the same configuration as the input pin, as they both have 6 pins. Refer to Digital Main Pinout.

### Interface
The 10 pin dupont wire used for the interface is also colour coded, which ideally should not be changed but is listed solely for reference:
*NB. the colours are those of the cable, not the wires soldered on the board*

| Dupont Pin (left-to-right) | Teensy Pin | Usage         | Colour |
|----------------------------|------------|---------------|--------|
| 1                          | 36         | Select Button | Brown  |
| 2                          | 37         | Rotary Button | Red    |
| 3                          | 33         | Rotary B      | Orange |
| 4                          | 34         | Rotary A      | Yellow |
| 5                          | 19         | SCL           | Green  |
| 6                          | 18         | SDA           | Blue   |
| 7                          | 35         | LED GREEN     | Purple |
| 8                          | 39         | LED RED       | Gray   |
| 9                          | VCC        |               | White  |
| 10                         | GND        |               | Black  | 




## Software
To add compatibility for new sensors, a few steps need to be taken to include them in the software. While these instructions are meant to be able to be followed by anyone, basic knowledge of C++ or general programming is recommended.

### Adding menu entries
When adding menu entries, a distinction is made between a _MENU entry and a _VIEW or _ACTION entry. This is primarily due to the fact that a menu entry needs to be assigned differently in the menu hierarchy, as it itself has submenus.

The first step is adding the entry to the menu structure. This is done by first declaring it in the enum function and the defining its place in the menu hierarchy by adding it to the respective MenuItem array.

For submenus, a STATE_[name]_MENU should be added between the start and end comments in the function. For simpler view or action menus, a state should be added below the divider comment.
```
enum StateID {
  STATE_MENU_START,
  // START
  STATE_MAIN_MENU,
  STATE_SENSOR_MENU,
  STATE_SETTINGS_MENU,
  STATE_MEASURE_MENU,
  STATE_VIEW_CONFIG,
  STATE_MAINIMU_MENU,
  // END
  STATE_MENU_END,
// DIVIDER
  STATE_VIEW_ANGLES,
  STATE_VIEW_ACCEL,
  STATE_VIEW_GYRO,
  STATE_VIEW_MAG,
  STATE_ACTION_ZERO_RESET,
  STATE_VIEW_GPS,
  STATE_VIEW_STEER,
  STATE_VIEW_WHEELSPEED,
  STATE_MEASURING,
  STATE_VIEW_MEASUREMENTS,
  STATE_VIEW_RTC
};
```

Below is an example of a MenuItem array. For submenus, both an entry in the menu 'above' it, and a new MenuItem with the entries below it should be made. For view or action entries, only adding them to the array they are in is sufficient.

```
const MenuItem settingsMenu[] = {
  { "Set Zero", STATE_ACTION_ZERO_RESET },
  { "Info", STATE_MAIN_MENU },
  { "Load Config", STATE_VIEW_CONFIG },
  { "View RTC Time", STATE_VIEW_RTC }
};
```

### Adding sensor support

#### 1. Include libraries and declare variables
Add the required libraries to the #include in the first few lines of the code and define global variables.

#### 2. Update applyConfig() 
Add the sensor name logic to the if statement

#### 3. Initialize the sensor in setup()
Edit the code below to fit the new sensor and add it to setup(); depending on the sensor, either INPUT or INPUT_PULLUP should be used.
```
  if (isSteerAngleConfigured && steerPin != -1) {
    pinMode(steerPin, INPUT);
    Serial.print("Steer angle sensor configured to pin ");
    Serial.println(steerPin);
  }
```
#### 4. Read sensor data in loop()
Add a function to read the sensor data in loop(), below is an example function for the steer angle sensor.
```
if (isSteerAngleConfigured && steerPin != -1) {
    potVal = analogRead(steerPin);
    steerVal = map(potVal, 0, 1023, steerMin * 100, steerMax * 100) / 100.0;
  }
```

#### 5. Write data to CSV
Add the sensor variable to the list in saveDataToFile()
```
dataFile.print(steerVal, 2); dataFile.print(",");
```


### Adding config variables

#### 1. Declare a global variable
Declare the variable to be used globally at the top of the file.

#### 2. Update `applyConfig()`
Add an `else if(key == "xxxxx")` with your config key and add the associated logic.

#### 3. Add entry to SD card
Add a line for the setting to be enabled/disable or add the sensor to the list of available sensors in `config.txt`

#### 4. Implement feature
Implement the setting or pin assignment.
