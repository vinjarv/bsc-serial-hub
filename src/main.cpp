#include <Arduino.h>


// Serial Hub for Pi Pico
// Splits USB Serial to several ports

// Protocol:  <port number> <command>\n
// Returns:   <port number> <data>\n
// Data might be out of order for different ports, but in order for each port
// I.e. a command to port 1 might not return for a while, leading to a command to port 2 replying first


// USB Serial is Serial
// UART0 is Serial1
// UART1 is Serial2
// Rest is software defined serial on PIO modules
uint tx_pins[] = 
{
  D0,   // Port 1
  D8,   // Port 2
  D2,   // Port 3
  D4,   // Port 4
  D6,   // Port 5
  D10   // Port 6 
};
uint rx_pins[] =
{
  D1,   // Port 1
  D9,   // Port 2
  D3,   // Port 3
  D5,   // Port 4
  D7,   // Port 5
  D11   // Port 6
};

// Define PIO ports to allow for a total of 6 bidirectional ports
const uint BUFFER_SIZE = 32;
SerialPIO serial_pios[4] = 
{
  SerialPIO(tx_pins[2], rx_pins[2], BUFFER_SIZE),
  SerialPIO(tx_pins[3], rx_pins[3], BUFFER_SIZE),
  SerialPIO(tx_pins[4], rx_pins[4], BUFFER_SIZE),
  SerialPIO(tx_pins[5], rx_pins[5], BUFFER_SIZE)
};

// User defined buffers to store the incoming data
// Used to avoid waiting for a transmission to finish, as we don't know the length of the incoming data,
// and there doesn't seem to be a way to check if a newline has been received
String incoming_port_strings[6]; 

const uint FAN_PWM_PIN = D16;

void setup() {
  Serial.begin(115200);
  Serial.println();
  
  Serial1.setTX(tx_pins[0]);
  Serial1.setRX(rx_pins[0]);
  Serial1.begin(115200);

  Serial2.setTX(tx_pins[1]);
  Serial2.setRX(rx_pins[1]);
  Serial2.begin(115200);

  serial_pios[0].begin(115200);
  serial_pios[1].begin(115200);
  serial_pios[2].begin(9600);   // Two last ports are slower
  serial_pios[3].begin(9600);   

  pinMode(FAN_PWM_PIN, OUTPUT);
  analogWriteFreq(25000);   // 25kHz, from https://noctua.at/pub/media/wysiwyg/Noctua_PWM_specifications_white_paper.pdf
  analogWriteRange(100);    // 0-100% duty cycle
  analogWrite(FAN_PWM_PIN, 100-0); // Connected to NOT gate, so 100% duty cycle is off
}

void handle_local_io(String& str) {
  int separator_index = str.indexOf(' ');
  // No separator, wrong command
  if (separator_index < 0) {
    Serial.println();
    return;
  } 
  String command = str.substring(0, separator_index);
  int value = str.substring(separator_index + 1).toInt();
  
  if (command == "fan") {
    if (value < 0 || value > 100) {
      Serial.println();
      return;
    }
    analogWrite(FAN_PWM_PIN, 100 - value); // Connected to NOT gate, so 100% duty cycle is off
  }
}

void pass_command(String& str) {
  int separator_index = str.indexOf(' ');
  // No separator, wrong command
  if (separator_index < 0) {
    Serial.println();
    str = "";
    return;
  }

  uint port_num = str.substring(0, separator_index).toInt();
  String command = str.substring(separator_index + 1);

  // Port number out of range
  if (port_num < 0 || port_num > 6) {
    Serial.println();
    str = "";
    return;
  }
  
  // Port number 0, handle locally
  if (port_num == 0) {
    handle_local_io(command);
  }
  // Port number 1 or 2, send to Serial1 or Serial2
  if (port_num == 1) {
    Serial1.println(command);
  } else if (port_num == 2) {
    Serial2.println(command);
  } else {
  // Port number 3-6, send to SerialPIO
    serial_pios[port_num - 3].println(command);
  }
  str = "";
}

void handle_usb() {
  static String usb_string = "";
  while (Serial.available() > 0) {
    char incoming_char = (char)Serial.read();
    if (incoming_char == '\n') {
      usb_string.trim();
      // Handle command
      pass_command(usb_string);
    } else {
      usb_string += incoming_char;
    }
    // If the buffer is full, command was too long or something went wrong. Clear buffer
    if (usb_string.length() >= BUFFER_SIZE) {
      usb_string = "";
      Serial.println();
    }
  } 
}

void loop() {
  // Read and handle commands from USB
  handle_usb();

  // Handle incoming data on Serial1
  while (Serial1.available() > 0) {
    char incoming_char = (char)Serial1.read();
    if (incoming_char == '\n') {
      incoming_port_strings[0].trim();
      Serial.println("1 " + incoming_port_strings[0]);
      incoming_port_strings[0] = "";
    } else {
      incoming_port_strings[0] += incoming_char;
    }
    if (incoming_port_strings[0].length() >= BUFFER_SIZE) {
      incoming_port_strings[0] = "";
      Serial1.println();
    }
  }

  // Handle incoming data on Serial2
  while (Serial2.available() > 0) {
    char incoming_char = (char)Serial2.read();
    if (incoming_char == '\n') {
      incoming_port_strings[1].trim();
      Serial.println("2 " + incoming_port_strings[0]);
      incoming_port_strings[1] = "";
    } else {
      incoming_port_strings[1] += incoming_char;
    }
    if (incoming_port_strings[1].length() >= BUFFER_SIZE) {
      incoming_port_strings[1] = "";
      Serial2.println();
    }
  }

  // Handle incoming data on SerialPIOs
  for (uint i = 0; i < 4; i++) {
    while (serial_pios[i].available() > 0) {
      char incoming_char = (char)serial_pios[i].read();
      if (incoming_char == '\n') {
        incoming_port_strings[i+2].trim();
        Serial.println(String(i+3) + " " + incoming_port_strings[i+2]);
        incoming_port_strings[i+2] = "";
      } else {
        incoming_port_strings[i+2] += incoming_char;
      }
      if (incoming_port_strings[i+2].length() >= BUFFER_SIZE) {
        incoming_port_strings[i+2] = "";
        serial_pios[i].println();
      }
    }
  }
}
