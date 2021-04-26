<?php

$servername = "localhost";

// REPLACE with your Database name
$dbname = "esp_data";
// REPLACE with Database user
$username = "root";
// REPLACE with Database user password
$password = "yourPWD";

// Keep this API Key value to be compatible with the ESP32 code provided in the project page. 
// If you change this value, the ESP32 sketch needs to match
$api_key_value = "tPmAT5Ab3j7F9";

$api_key= $sensor = $location = $t_dht = $h_dht = $p_bmp = $t_bmp = $t_ds18 = $Day = $Month = $Time = "";
//$api_key= $sensor = $location = $t_dht = $h_dht = "";

if ($_SERVER["REQUEST_METHOD"] == "POST") {
    $api_key = test_input($_POST["api_key"]);
    if($api_key == $api_key_value) {
        $sensor = test_input($_POST["sensor"]);
        $location = test_input($_POST["location"]);
        $t_dht = test_input($_POST["t_dht"]);
        $h_dht = test_input($_POST["h_dht"]);
        $p_bmp = test_input($_POST["p_bmp"]);
        $t_bmp = test_input($_POST["t_bmp"]);
        $t_ds18 = test_input($_POST["t_ds18"]);
        $Day = test_input($_POST["Day"]);
        $Month = test_input($_POST["Month"]);
        $Time = test_input($_POST["Time"]);
        
        // Create connection
        $conn = new mysqli($servername, $username, $password, $dbname);
        // Check connection
        if ($conn->connect_error) {
            die("Connection failed: " . $conn->connect_error);
        } 
        
        $sql = "INSERT INTO Meteo_Indoor (sensor,location,t_dht, h_dht, p_bmp,t_bmp,t_ds18,Day,Month,Time)
        VALUES ('" . $sensor . "', '" . $location . "','" . $t_dht . "', '" . $h_dht . "', '" . $p_bmp . "', '" . $t_bmp . "', '" . $t_ds18 . "', '" . $Day . "', '" . $Month . "', '" . $Time . "')";
 
        
        if ($conn->query($sql) === TRUE) {
            echo "New record created successfully";
        } 
        else {
            echo "Error: " . $sql . "<br>" . $conn->error;
        }
    
        $conn->close();
    }
    else {
        echo "Wrong API Key provided.";
    }

}
else {
    echo "No data posted with HTTP POST.";
}

function test_input($data) {
    $data = trim($data);
    $data = stripslashes($data);
    $data = htmlspecialchars($data);
    return $data;
}
