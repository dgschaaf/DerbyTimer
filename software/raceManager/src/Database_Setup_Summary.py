## Mocking Arduino Data

## bash - Input race data manually
python mock_bluetooth.py "R001_LT10321"


## txt - Read from a pre-written file simulating race events
R001_LT10321
R001_LR00567
R002_RT09876
R002_RR00789


## python - Generate random data for testing
import random

def generate_mock_data(race_id, track):
    metric = random.choice(['LT', 'LR', 'RT', 'RR'])
    time = random.randint(5000, 15000)  # Mock time in milliseconds
    return f"{race_id}_{metric}{time}"

print(generate_mock_data('R001', 'Left'))

## bash - Create a database file 
sqlite3 race_data.db

## sql - create table for storing race data
CREATE TABLE race_results (
    Race_ID TEXT,
    Car_ID INTEGER,
    Track TEXT,
    Track_Time REAL,
    Reaction_Time REAL,
    Car_Time REAL,
    Timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (Race_ID, Car_ID, Track)
);

## bash - create python script to insert mock data
nano insert_mock_data.py

## python - add folowing code to script
import sqlite3
from datetime import datetime

# Connect to SQLite database (creates the file if it doesn't exist)
conn = sqlite3.connect('race_data.db')
cursor = conn.cursor()

# Function to insert mock data
def insert_mock_data():
    data = [
        ("R001", 101, "Left", 10.321, 0.567, 9.754),
        ("R001", 102, "Right", 9.876, 0.789, 9.087),
        ("R002", 103, "Left", 11.432, None, None),
        ("R002", 104, "Right", None, None, None),
    ]

    for row in data:
        cursor.execute('''
            INSERT OR REPLACE INTO race_results (
                Race_ID, Car_ID, Track, Track_Time, Reaction_Time, Car_Time
            ) VALUES (?, ?, ?, ?, ?, ?)
        ''', row)

    conn.commit()

# Call the function
insert_mock_data()

# Verify the insertion
cursor.execute('SELECT * FROM race_results')
rows = cursor.fetchall()
for row in rows:
    print(row)

# Close the connection
conn.close()

## bash - run script to ensure it works
chmod +x insert_mock_data.py
python3 insert_mock_data.py
sqlite3 race_data.db
## sql
SELECT * FROM race_results;
.quit

## bash - write script for mock bluetooth handling
nano mock_bluetooth.py

## python - add following code
import sqlite3
import re
from datetime import datetime

# Connect to SQLite database
conn = sqlite3.connect('race_data.db')
cursor = conn.cursor()

# Function to parse and validate race data
def parse_race_data(data):
    # Example format: R001_LT10321 (Race ID + Metric)
    match = re.match(r'^(R\d{3})_(LT|LR|RT|RR)(\d{5})$', data)
    if match:
        race_id = match.group(1)
        metric = match.group(2)
        value = int(match.group(3)) / 1000  # Convert to seconds
        return race_id, metric, value
    else:
        print("Invalid data format:", data)
        return None

# Function to insert or update data in the database
def update_database(race_id, metric, value):
    # Determine track and column based on metric
    track = "Left" if metric in ["LT", "LR"] else "Right"
    column = "Track_Time" if metric in ["LT", "RT"] else "Reaction_Time"

    # Update the appropriate row
    cursor.execute(f'''
        INSERT INTO race_results (Race_ID, Car_ID, Track, {column})
        VALUES (?, ?, ?, ?)
        ON CONFLICT (Race_ID, Car_ID, Track)
        DO UPDATE SET {column} = excluded.{column}
    ''', (race_id, 0, track, value))  # Replace '0' with actual Car_ID when ready

    # Calculate Car_Time if both Track_Time and Reaction_Time are present
    cursor.execute(f'''
        SELECT Track_Time, Reaction_Time FROM race_results
        WHERE Race_ID = ? AND Track = ?
    ''', (race_id, track))
    row = cursor.fetchone()
    if row and row[0] is not None and row[1] is not None:
        car_time = row[0] - row[1]
        cursor.execute('''
            UPDATE race_results SET Car_Time = ?
            WHERE Race_ID = ? AND Track = ?
        ''', (car_time, race_id, track))

    conn.commit()

# Mock input data
mock_data = [
    "R001_LT10321",
    "R001_LR00567",
    "R002_RT09876",
    "R002_RR00789"
]

# Process each mock input
for entry in mock_data:
    parsed = parse_race_data(entry)
    if parsed:
        update_database(*parsed)

# Display the database content
cursor.execute("SELECT * FROM race_results")
rows = cursor.fetchall()
for row in rows:
    print(row)

# Close the connection
conn.close()

## bash - run script to ensure it works
chmod +x mock_bluetooth.py
python3 mock_bluetooth.py
sqlite3 race_data.db
## sql
SELECT * FROM race_results;
.quit

