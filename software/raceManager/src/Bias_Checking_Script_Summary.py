## makefile
Bias = Avg_Left_Car_Time - Avg_Right_Car_Time

## sql
SELECT 
    Car_ID, 
    Track,
    AVG(Car_Time) AS Avg_Car_Time
FROM race_results
GROUP BY Car_ID, Track;

## sql
SELECT 
    L.Car_ID,
    L.Avg_Car_Time AS Avg_Left_Time,
    R.Avg_Car_Time AS Avg_Right_Time,
    (L.Avg_Car_Time - R.Avg_Car_Time) AS Bias
FROM (
    SELECT Car_ID, AVG(Car_Time) AS Avg_Car_Time
    FROM race_results
    WHERE Track = 'Left'
    GROUP BY Car_ID
) L
JOIN (
    SELECT Car_ID, AVG(Car_Time) AS Avg_Car_Time
    FROM race_results
    WHERE Track = 'Right'
    GROUP BY Car_ID
) R
ON L.Car_ID = R.Car_ID;

## python
import sqlite3
import pandas as pd

# Connect to the database
conn = sqlite3.connect('race_data.db')

# Load bias data into a DataFrame
query = """
SELECT 
    L.Car_ID,
    L.Avg_Car_Time AS Avg_Left_Time,
    R.Avg_Car_Time AS Avg_Right_Time,
    (L.Avg_Car_Time - R.Avg_Car_Time) AS Bias
FROM (
    SELECT Car_ID, AVG(Car_Time) AS Avg_Car_Time
    FROM race_results
    WHERE Track = 'Left'
    GROUP BY Car_ID
) L
JOIN (
    SELECT Car_ID, AVG(Car_Time) AS Avg_Car_Time
    FROM race_results
    WHERE Track = 'Right'
    GROUP BY Car_ID
) R
ON L.Car_ID = R.Car_ID;
"""

df = pd.read_sql_query(query, conn)

# Calculate bias statistics
mean_bias = df['Bias'].mean()
median_bias = df['Bias'].median()
std_bias = df['Bias'].std()

print(f"Mean Bias: {mean_bias:.3f}")
print(f"Median Bias: {median_bias:.3f}")
print(f"Standard Deviation of Bias: {std_bias:.3f}")

conn.close()
