#include <WiFi.h>
#include <PubSubClient.h>

const int BUILTIN_LED = 32;

float batteryLevel = 100.0;
int motorSpeed = 50;
unsigned long batteryTimer = 0;

//Network information
const char* ssid = "RED_VELVET 4802";
const char* password = "03175B|j";

//MQTT Broker IP address
const char* mqtt_server = "84.52.229.122";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
char msg[50];

class BankAccount {
    private:
        int balance = 0;
    public:
        BankAccount() { 
            client.publish("zumo/bank/getBalance", "1337"); //request the balance from the server with pin 1337
        }
        void transfer(int amount, char* toAccount) {
            if (balance >= amount) {
                char tempString[50];
                itoa(amount, tempString, 10);
                strcat(tempString, "1337"); //Append pin to the string
                if (toAccount == "charger") {
                    client.publish("zumo/bank/transfer/charger", tempString);
                }
                else {
                    Serial.println("Account not in local database");
                }
                this->requestServerBalance();
            }
            else {
                Serial.println("Not enough money");
            }
        }
        void withdraw(int amount) {
            balance -= amount;
        }
        int getBalance() {
            return balance;
        }
        void updateLocalBalance(int amount) {
            balance = amount;
        }
        void requestServerBalance() {
            client.publish("zumo/bank/getBalance", "1337");
        }
};
BankAccount account;

void setup_wifi() 
{
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) 
{
    Serial.print("Message arrived on topic: ");
    Serial.print(topic);
    Serial.print(". Message: ");
    String messageTemp;

    for (int i = 0; i < length; i++) 
    {
        Serial.print((char)message[i]);
        messageTemp += (char)message[i];
    }
    Serial.println();

    // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
    // Changes the output state according to the message
    if (String(topic) == "esp32/output") {
        Serial.print("Changing output to ");
        if (messageTemp == "on") 
        {
            Serial.println("on");
            digitalWrite(BUILTIN_LED, HIGH);
        }
        else if (messageTemp == "off") 
        {
            Serial.println("off");
            digitalWrite(BUILTIN_LED, LOW);
        }
    }
    if (String(topic) == "zumo/bank/currentBalance") {
        Serial.print("Current balance: ");
        Serial.println (messageTemp);
        int currentBalance = messageTemp.toInt();
        account.updateLocalBalance(currentBalance);
    }
    if (String(topic) == "zumo/bank/error") {
        Serial.print("Error: ");
        Serial.println(messageTemp);
    }
}

void reconnect() 
{
    // Loop until we're reconnected
    while (!client.connected()) 
    {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("ESP32Client")) 
        {
            Serial.println("connected");
            // Subscribe or resubscribe to a topic
            client.subscribe("esp32/output");
            client.subscribe("zumo/bank/currentBalance");
            client.subscribe("zumo/bank/error");            
        } 
        else 
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void calculateBatteryLevel() {
    float speedFactor = 0.01;
    while (Serial2.available() > 0) 
    {
        motorSpeed = Serial2.parseInt();
    }
    unsigned long now = millis();
    if (now - batteryTimer > 1000) 
    {
        batteryTimer = now;
        batteryLevel -= motorSpeed * speedFactor; //P=mv
    }
    if (batteryLevel < 0) 
    {
        batteryLevel = 0;
        //TODO: Send message to zumo to stop
    }
}

void setup() 
{
    Serial.begin(115200);
    Serial2.begin(9600); //Start serial communication with Zumo

    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    pinMode(BUILTIN_LED, OUTPUT);
}

void loop() 
{
    calculateBatteryLevel();

    if (!client.connected()) 
    {
        reconnect();
    }
    client.loop();

    unsigned long now = millis();
    if (now - lastMsg > 1000) 
    {
        lastMsg = now;

        char tempString[3];
        itoa(motorSpeed, tempString, 10);        
        client.publish("zumo/speed", tempString);

        char tempString2[6];
        dtostrf(batteryLevel, 1, 2, tempString2);
        client.publish("zumo/battery", tempString2);
    
        Serial.println(account.getBalance());
    }
}