#include "drivers.hpp"
#include "devices.hpp"

#define WIFI_NAME "RP"
#define WIFI_SSID "1234567890"

uart gps_uart;
Pit timer;
Nvs storage;
Pit mqtt_timer;

TinyGPS gps;
MQTT mqtt;

uint8_t month, day, hour, minute, second;
int year;

PWM servo_pin = PWM(GPIO_NUM_15, LEDC_TIMER_12_BIT, 50);
servo m_servo = servo(servo_pin);

bool servo_state = false; // false = destrancado, true = trancado.

void messageHandler(const char *topic, const char *data)
{
    printf("Recebido do topico %s\n %s", topic, data);

    if (strcmp(topic, "esp32/gps-servo") == 0)
    {
        servo_state = !servo_state; // faz o toggle

        if (servo_state)
        {
            printf("Servo -> MAX\n");
            m_servo.write(1.0); // posição máxima
        }
        else
        {
            printf("Servo -> MIN\n");
            m_servo.write(-1.0); // posição mínima
        }
    }
}

const int dias_mes[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void adjust_timezone(int &year, uint8_t &month, uint8_t &day, uint8_t &hour, int fuso);

extern "C" void app_main()
{
    gps_uart.init(UART_NUM_2, 9600, 17, 16);
    timer.init();
    timer.start();
    storage.init();

    m_servo.init(1.0, 2.0);
    m_servo.set_power(1);
    m_servo.set_max_power(1);

    // wifi_start("CIMATEC-VISITANTE", "");


    wifi_start(WIFI_NAME, WIFI_SSID);

    while (!wifi_connected())
    {
        vTaskDelay(1);

        if (timer.read())
        {
            timer.write(1000);
            printf("Connecting to WiFi...\n");
        }
    }

    mqtt.init(1883, "mqtt://test.mosquitto.org");
    mqtt.onMessage(messageHandler);
    mqtt.read("esp32/gps-servo");

    gps.init();

    while (1)
    {

        bool newData = false;
        uint8_t c;

        while (gps_uart.data_len())
        {
            gps_uart.read(&c, 1);
            if (gps.encode(c))
                newData = true;
        }

        if (newData)
        {
            float flat, flon;
            gps.f_get_position(&flat, &flon);

            gps.crack_datetime(&year, &month, &day, &hour, &minute, &second);
            adjust_timezone(year, month, day, hour, -3); // UTC-3

            printf("LAT=%f LON=%f SAT=%d PREC=%lu\t",
                   flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flat,
                   flon == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flon,
                   gps.satellites() == TinyGPS::GPS_INVALID_SATELLITES ? 0 : gps.satellites(),
                   gps.hdop() == TinyGPS::GPS_INVALID_HDOP ? 0 : gps.hdop());

            printf("DATE=%02d/%02d/%04d TIME=%02d:%02d:%02d\n",
                   day,
                   month,
                   year,
                   hour,
                   minute,
                   second);

            storage["latitude"] = flat;
            storage["longitude"] = flon;
            storage["gps_hdop"] = gps.hdop() * 4.0 / 100;
            storage["gps_year"] = (float)year;
            storage["gps_month"] = (float)month;
            storage["gps_day"] = (float)day;
            storage["gps_hour"] = (float)hour;
            storage["gps_minute"] = (float)minute;
            storage["gps_second"] = (float)second;
        }

        if (timer.read())
        {
            float latitude = storage["latitude"];
            float longitude = storage["longitude"];
            float hdop = storage["gps_hdop"];
            int year = (int)storage["gps_year"];
            uint8_t month = (uint8_t)storage["gps_month"];
            uint8_t day = (uint8_t)storage["gps_day"];
            uint8_t hour = (uint8_t)storage["gps_hour"];
            uint8_t minute = (uint8_t)storage["gps_minute"];
            uint8_t second = (uint8_t)storage["gps_second"];

            char payload[200];
            sprintf(payload,
                    "{\"latitude\": %.6f, \"longitude\": %.6f, \"timestamp\": \"%04d-%02d-%02d %02d:%02d:%02d\", \"error\": %.6f}",
                    latitude,
                    longitude,
                    year,
                    month,
                    day,
                    hour,
                    minute,
                    second,
                    hdop);
            mqtt.write("esp32/gps", payload);

            timer.write(10000); // 10 seconds
        }

        vTaskDelay(1);
    }
}

void adjust_timezone(int &year, uint8_t &month, uint8_t &day, uint8_t &hour, int fuso)
{
    int year_adjust, month_adjust, day_adjust, hour_adjust;

    year_adjust = year;
    month_adjust = month;
    day_adjust = day;
    hour_adjust = hour;

    hour_adjust += fuso;

    if (hour_adjust < 0)
    {
        hour_adjust += 24;
        day_adjust--;

        if (day_adjust <= 0)
        {

            month_adjust--;

            if (month_adjust < 1)
            {
                month_adjust = 12;
                year_adjust--;
            }

            day_adjust = dias_mes[month_adjust - 1];

            // ajuste de fevereiro em ano bissexto
            if (month_adjust == 2)
            {
                bool leap = (year_adjust % 4 == 0 && (year_adjust % 100 != 0 || year_adjust % 400 == 0));
                if (leap)
                    day_adjust = 29;
            }
        }
    }

    if (hour_adjust >= 24)
    {
        hour_adjust -= 24;
        day_adjust++;

        // ajuste para virar mês
        int last_day = dias_mes[month_adjust - 1];

        if (month_adjust == 2)
        {
            bool leap = (year_adjust % 4 == 0 && (year_adjust % 100 != 0 || year_adjust % 400 == 0));
            if (leap)
                last_day = 29;
        }

        if (day_adjust > last_day)
        {
            day_adjust = 1;
            month_adjust++;
            if (month_adjust > 12)
            {
                month_adjust = 1;
                year_adjust++;
            }
        }
    }

    year = year_adjust;
    month = month_adjust;
    day = day_adjust;
    hour = hour_adjust;
}