<!DOCTYPE html>
<html><body>
<?php

$servername = "localhost";

// REPLACE with your Database name
$dbname = "esp_data";
// REPLACE with Database user
$username = "root";
// REPLACE with Database user password
$password = "yourPWD";

// Create connection
$conn = new mysqli($servername, $username, $password, $dbname);
// Check connection
if ($conn->connect_error) {
    die("Connection failed: " . $conn->connect_error);
} 

$sql = "SELECT id, sensor, location, t_dht, h_dht, p_bmp, Day, Month, Time, reading_time FROM Meteo_Indoor ORDER BY id DESC";


echo '<table cellspacing="5" cellpadding="5">
      <tr> 
        <td>ID</td> 
        <td>Location</td> 
        <td>Temp</td>
        <td>Hum</td> 
        <td>Pressure</td>
        <td>Day</td>
        <td>Month</td>
        <td>Time</td>
        <td>Timestamp</td> 
      </tr>';
 
if ($result = $conn->query($sql)) {
    while ($row = $result->fetch_assoc()) {
        $row_id = $row["id"];
        //$row_sensor = $row["sensor"];
        $row_location = $row["location"];
        $row_t_dht = $row["t_dht"];
        //$row_t_bmp = $row["t_bmp"];
        //$row_t_ds18 = $row["t_ds18"];
        $row_h_dht = $row["h_dht"]; 
        $row_p_bmp = $row["p_bmp"];  
        $row_Day = $row["Day"];
        $row_Month = $row["Month"];
        $row_Time = $row["Time"];   
        $row_reading_time = $row["reading_time"];
        // Uncomment to set timezone to - 1 hour (you can change 1 to any number)
        //$row_reading_time = date("Y-m-d H:i:s", strtotime("$row_reading_time - 1 hours"));
      
        // Uncomment to set timezone to + 4 hours (you can change 4 to any number)
        //$row_reading_time = date("Y-m-d H:i:s", strtotime("$row_reading_time + 4 hours"));
      
        echo '<tr> 
                <td>' . $row_id . '</td> 
                <td>' . $row_location . '</td> 
                <td>' . $row_t_dht. '</td> 
                <td>' . $row_h_dht . '</td>
                <td>' . $row_p_bmp . '</td>
                <td>' . $row_Day. '</td> 
                <td>' . $row_Month . '</td>
                <td>' . $row_Time . '</td>
                <td>' . $row_reading_time . '</td> 
              </tr>';
    }
    $result->free();
}

$conn->close();
?> 
</table>
</body>
</html>
