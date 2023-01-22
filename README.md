# Smart-Parking-Internet-of-Things-Application

Project for the Wireless Mobile Networks Course (https://fenix.tecnico.ulisboa.pt/disciplinas/RMSF364511132646/2020-2021/2-semestre/standalone-projects)

This project is a Smart Parking IoT application developed as a way to remotely reserve and check the real-time availability of nearby parking spots, through an android mobile application.

In order to achieve the objectives of this project a sensor/actuator system using an ESP8266 arduino board was implemented, containing a luminosity and a ultrassonic distance sensor. Each parking spot monitored by this application contains the system/actuator system, referenced above.

Each sensor system periodically sends luminosity and distance data to a cloud server that determines the availability status of each parking spot. The sensor system also transmits data to the cloud server everytime there is a luminosity/distance over a defined threshold. 

A more detailed description of the developed application can be found in the smart_parking_final_report_group_10.pdf file.

The sensor/actuator code can be found in the esp8266_software folder. Unfortunately, the code for the mobile application, developed using Android Studio, and for the cloud server, developed using the Back4App application, cannot be found in this repository.
