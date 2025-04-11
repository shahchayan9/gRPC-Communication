# Mini 2: Gathering Coordination using Shared Memory

This project implements a distributed system with multiple processes communicating via gRPC and utilizing shared memory for caching between processes. The system is designed as an overlay network of processes (A, B, C, D, E) across multiple computers.

## 📁 Project Structure

```
mini2_project/
├── src/                    # Source code
│   ├── process_a/          # Process A source code (Portal)
│   ├── process_b/          # Process B source code
│   ├── process_c/          # Process C source code
│   ├── process_d/          # Process D source code
│   ├── process_e/          # Process E source code
│   ├── common/             # Shared code and utilities
├── proto/                  # Protocol buffer definitions
├── build/                  # Build artifacts
├── config/                 # Configuration files
├── scripts/                # Build and run scripts
├── python_client/          # Python client for interacting with Process A
└── tests/                  # Unit and integration tests
```

## ✅ Requirements

- C++17 compiler (gcc or clang)
- CMake (version 3.10 or higher)
- gRPC and Protocol Buffers
- Python 3.6 or higher (for Python client)
- Python packages: `grpcio`, `grpcio-tools`, `protobuf`

## 🚀 Setup

### 1. Install dependencies on each Mac:

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install required dependencies
brew install cmake
brew install protobuf
brew install grpc

# Install Python dependencies
pip3 install grpcio grpcio-tools protobuf
```

### 2. Clone the repository on each Mac:

```bash
git clone <repository-url>
cd mini2_project
```

### 3. Update the configuration file:

Edit `config/network_config.json` to match your network configuration. Replace the IP addresses with the actual IPs of your Mac systems:

- **Mac 1**: Processes A and B  
- **Mac 2**: Processes C, D and E

---

## 🛠️ Building the Project

Run the build script:

```bash
./scripts/build.sh
```

This will generate the Protocol Buffer code, configure CMake, and build the project.

---

## 🧠 Running the System

### 🧩 Distribution of Processes

- **Mac 1**: Processes A and B  
- **Mac 2**: Processes C, D and E  

### ▶️ Starting Processes

**Mac 1:**

```bash
# Terminal 1
./scripts/run_process_a.sh

# Terminal 2
./scripts/run_process_b.sh
```

**Mac 2:**

```bash
# Terminal 1
./scripts/run_process_c.sh

# Terminal 2
./scripts/run_process_d.sh

# Terminal 3
./scripts/run_process_e.sh
```

---

## 🐍 Using the Python Client

Use the Python client to interact with Process A:

```bash
# Query all data
./scripts/run_client.sh get_all

# Query data by key
./scripts/run_client.sh get_by_key key1 key2 key3

# Query data by prefix
./scripts/run_client.sh get_by_prefix prefix
```

---

## 🧱 System Architecture

### 🔁 Process Roles

- **Process A**: Entry point/portal that receives client requests
- **Process B**: Backend process with data subset 1
- **Process C**: Backend process with data subset 2
- **Process D**: Backend process with data subset 3
- **Process E**: Backend process with data subset 4

### 🌐 Overlay Network

The system is configured with the following overlay connections:

- AB: Process A connects to Process B  
- BC: Process B connects to Process C  
- BD: Process B connects to Process D  
- CE: Process C connects to Process E  
- DE: Process D connects to Process E

---

## ⚡ Caching Mechanism

Each process implements a shared memory-based cache to store query results. This transactionless caching mechanism reduces the need for request-response communication by storing frequently accessed data in shared memory.

---

## 🧪 Testing and Verification

To verify that the system is working correctly:

1. Start all processes in the order described above.
2. Use the Python client to query data.
3. Check the logs of each process to see how queries are processed and forwarded.
4. Verify that caching is working by sending the same query multiple times.
