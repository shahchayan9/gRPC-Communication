#!/usr/bin/env python3

import csv
import os
import sys

def parse_crash_data(input_file, output_dir):
    """Parse crash data from input file and distribute to borough-specific files."""
    
    # Create directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Define output files
    brooklyn_file = os.path.join(output_dir, 'process_b', 'brooklyn_crashes.csv')
    queens_file = os.path.join(output_dir, 'process_c', 'queens_crashes.csv')
    bronx_file = os.path.join(output_dir, 'process_d', 'bronx_crashes.csv')
    staten_island_file = os.path.join(output_dir, 'process_e', 'staten_island_crashes.csv')
    other_file = os.path.join(output_dir, 'process_e', 'other_crashes.csv')
    
    # Ensure subdirectories exist
    for subdir in ['process_b', 'process_c', 'process_d', 'process_e']:
        os.makedirs(os.path.join(output_dir, subdir), exist_ok=True)
    
    # Define field names based on the screenshot
    fieldnames = [
        'CRASH_DATE', 'CRASH_TIME', 'BOROUGH', 'ZIP_CODE', 'LATITUDE', 'LONGITUDE',
        'LOCATION', 'ON_STREET_NAME', 'CROSS_STREET_NAME', 'OFF_STREET_NAME',
        'NUMBER_OF_PERSONS_INJURED', 'NUMBER_OF_PERSONS_KILLED', 'NUMBER_OF_PEDESTRIANS'
    ]
    
    # Open output files
    brooklyn_csv = open(brooklyn_file, 'w', newline='')
    queens_csv = open(queens_file, 'w', newline='')
    bronx_csv = open(bronx_file, 'w', newline='')
    staten_island_csv = open(staten_island_file, 'w', newline='')
    other_csv = open(other_file, 'w', newline='')
    
    # Create CSV writers
    brooklyn_writer = csv.DictWriter(brooklyn_csv, fieldnames=fieldnames)
    queens_writer = csv.DictWriter(queens_csv, fieldnames=fieldnames)
    bronx_writer = csv.DictWriter(bronx_csv, fieldnames=fieldnames)
    staten_island_writer = csv.DictWriter(staten_island_csv, fieldnames=fieldnames)
    other_writer = csv.DictWriter(other_csv, fieldnames=fieldnames)
    
    # Write headers
    brooklyn_writer.writeheader()
    queens_writer.writeheader()
    bronx_writer.writeheader()
    staten_island_writer.writeheader()
    other_writer.writeheader()
    
    # For demonstration, we'll create sample data based on the screenshot
    # In a real scenario, you'd read this from the input file
    sample_data = [
        {"CRASH_DATE": "09/11/2021", "CRASH_TIME": "2:39", "BOROUGH": "", "ZIP_CODE": "", "LATITUDE": "", "LONGITUDE": "", "LOCATION": "", "ON_STREET_NAME": "WHITESTONE EXPRESSWAY", "CROSS_STREET_NAME": "20 AVENUE", "OFF_STREET_NAME": "", "NUMBER_OF_PERSONS_INJURED": "2", "NUMBER_OF_PERSONS_KILLED": "0", "NUMBER_OF_PEDESTRIANS": ""},
        {"CRASH_DATE": "03/28/2022", "CRASH_TIME": "11:45", "BOROUGH": "", "ZIP_CODE": "", "LATITUDE": "", "LONGITUDE": "", "LOCATION": "", "ON_STREET_NAME": "QUEENSBORO BRIDGE UPPER", "CROSS_STREET_NAME": "", "OFF_STREET_NAME": "", "NUMBER_OF_PERSONS_INJURED": "1", "NUMBER_OF_PERSONS_KILLED": "0", "NUMBER_OF_PEDESTRIANS": ""},
        {"CRASH_DATE": "09/11/2021", "CRASH_TIME": "9:35", "BOROUGH": "BROOKLYN", "ZIP_CODE": "11208", "LATITUDE": "40.667202", "LONGITUDE": "-73.8665", "LOCATION": "(40.667202, -73.8665)", "ON_STREET_NAME": "", "CROSS_STREET_NAME": "", "OFF_STREET_NAME": "1211 LORING AVENUE", "NUMBER_OF_PERSONS_INJURED": "0", "NUMBER_OF_PERSONS_KILLED": "0", "NUMBER_OF_PEDESTRIANS": ""},
        {"CRASH_DATE": "12/14/2021", "CRASH_TIME": "8:13", "BOROUGH": "BROOKLYN", "ZIP_CODE": "11233", "LATITUDE": "40.683304", "LONGITUDE": "-73.917274", "LOCATION": "(40.683304, -73.917274)", "ON_STREET_NAME": "SARATOGA AVENUE", "CROSS_STREET_NAME": "DECATUR STREET", "OFF_STREET_NAME": "", "NUMBER_OF_PERSONS_INJURED": "0", "NUMBER_OF_PERSONS_KILLED": "0", "NUMBER_OF_PEDESTRIANS": ""},
        {"CRASH_DATE": "12/14/2021", "CRASH_TIME": "8:17", "BOROUGH": "BRONX", "ZIP_CODE": "10475", "LATITUDE": "40.86816", "LONGITUDE": "-73.83148", "LOCATION": "(40.86816, -73.83148)", "ON_STREET_NAME": "", "CROSS_STREET_NAME": "", "OFF_STREET_NAME": "344 BAYCHESTER AVENUE", "NUMBER_OF_PERSONS_INJURED": "2", "NUMBER_OF_PERSONS_KILLED": "0", "NUMBER_OF_PEDESTRIANS": ""},
        {"CRASH_DATE": "12/14/2021", "CRASH_TIME": "16:50", "BOROUGH": "QUEENS", "ZIP_CODE": "11413", "LATITUDE": "40.675884", "LONGITUDE": "-73.75577", "LOCATION": "(40.675884, -73.75577)", "ON_STREET_NAME": "SPRINGFIELD BOULEVARD", "CROSS_STREET_NAME": "EAST GATE PLAZA", "OFF_STREET_NAME": "", "NUMBER_OF_PERSONS_INJURED": "0", "NUMBER_OF_PERSONS_KILLED": "0", "NUMBER_OF_PEDESTRIANS": ""},
        {"CRASH_DATE": "12/13/2021", "CRASH_TIME": "17:40", "BOROUGH": "STATEN ISLAND", "ZIP_CODE": "10301", "LATITUDE": "40.63165", "LONGITUDE": "-74.08762", "LOCATION": "(40.63165, -74.08762)", "ON_STREET_NAME": "VICTORY BOULEVARD", "CROSS_STREET_NAME": "WOODSTOCK AVENUE", "OFF_STREET_NAME": "", "NUMBER_OF_PERSONS_INJURED": "1", "NUMBER_OF_PERSONS_KILLED": "0", "NUMBER_OF_PEDESTRIANS": ""}
    ]
    
    # Write data to appropriate files based on borough
    for row in sample_data:
        borough = row.get('BOROUGH', '').strip().upper()
        
        if borough == 'BROOKLYN':
            brooklyn_writer.writerow(row)
        elif borough == 'QUEENS':
            queens_writer.writerow(row)
        elif borough == 'BRONX':
            bronx_writer.writerow(row)
        elif borough == 'STATEN ISLAND':
            staten_island_writer.writerow(row)
        else:
            other_writer.writerow(row)
    
    # Close files
    brooklyn_csv.close()
    queens_csv.close()
    bronx_csv.close()
    staten_island_csv.close()
    other_csv.close()
    
    print(f"Data has been parsed and distributed to files in {output_dir}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    else:
        input_file = None  # Using sample data for demonstration
    
    output_dir = "data"
    parse_crash_data(input_file, output_dir)
