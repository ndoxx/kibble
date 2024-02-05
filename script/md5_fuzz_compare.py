#!/usr/bin/env python3

import subprocess
import hashlib
import random
import string

def generate_random_string(length=10):
    """Generate a random string of given length."""
    characters = string.ascii_letters + string.digits
    return ''.join(random.choice(characters) for _ in range(length))

def calculate_md5_using_kibble(input_string):
    """Calculate MD5 hash using the custom 'kmd5' program."""
    command = ["../bin/kmd5", input_string]
    result = subprocess.run(command, capture_output=True, text=True)
    return result.stdout.strip()

def calculate_md5_using_md5sum(input_string):
    """Calculate MD5 hash using the 'md5sum' Linux utility."""
    echo_ps = subprocess.Popen(["echo", "-n", input_string], stdout=subprocess.PIPE)
    result = subprocess.run(["md5sum"], stdin=echo_ps.stdout, capture_output=True, text=True)
    # skip last three characters
    return result.stdout.strip()[:-3]

def calculate_md5_using_hashlib(input_string):
    """Calculate MD5 hash using Python's hashlib."""
    md5 = hashlib.md5()
    md5.update(input_string.encode('utf-8'))
    return md5.hexdigest()

def fuzz_compare(min_length, max_length, num_tests):
    for length in range(min_length, max_length + 1):
        print(f"Testing strings of length {length}:")
        for _ in range(num_tests):
            random_string = generate_random_string(length)
            
            # Calculate MD5 using the custom 'nuclear' program
            nuclear_md5 = calculate_md5_using_kibble(random_string)

            # Calculate MD5 using the 'md5sum' Linux utility
            # ref_md5 = calculate_md5_using_md5sum(random_string)

            # Calculate MD5 using hashlib
            ref_md5 = calculate_md5_using_hashlib(random_string)

            # Compare the results
            if nuclear_md5 != ref_md5:
                print(f"Mismatch! String: {random_string}, Nuclear MD5: {nuclear_md5}, md5sum MD5: {ref_md5}")

if __name__ == "__main__":
    min_length = 50   # Minimum string length
    max_length = 4096 # Maximum string length
    num_tests = 100   # Number of tests for each length

    fuzz_compare(min_length, max_length, num_tests)
