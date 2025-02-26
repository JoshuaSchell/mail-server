# mail-server - setup guide

This guide provides instructions for setting up a PostgreSQL-based email ticket system that automatically sends emails when new tickets are inserted into the database, managed with Docker containers.

## Requirements

![image](https://github.com/user-attachments/assets/20b18bcd-8db7-4e9d-92a2-d76cfa407d47)

## Project Overview

This system consists of:
1. A PostgreSQL database for storing tickets
2. A C program that monitors the database for new tickets and sends emails
3. Docker containers to run both components

## Project Structure

```
project-root/
├── docker-compose.yml
├── .env                  # You will need to create this file!! (Everything else is provided)
├── postgres/
│   ├── Dockerfile
│   └── init-db/        
│       ├── 01-init.sql
│       └── 02-init.sh
└── email-sender/
    ├── Dockerfile
    ├── email-sender.c
    └── Makefile
```

## Step 1: Create Environment Configuration File

### .env File (DO NOT SHARE THIS FILE WITH ANYONE)

Create a `.env` file in the project root with credentials that will be used consistently across all services:

```
# PostgreSQL configuration
POSTGRES_HOST=postgres         # Standard default host for PostgreSQL 
POSTGRES_USER=postgres         # Standard default user for PostgreSQL 
POSTGRES_PASSWORD=postgres123  # Choose a secure password
POSTGRES_DB=ticketdb           # Name of the database to create
POSTGRES_PORT=5432             # Internal port, not the mapped port

# Gmail SMTP configuration
GMAIL_EMAIL=your-email@gmail.com        # Your Gmail email address
GMAIL_APP_PASSWORD=your-app-password    # App-specific password (not your regular password)
SMTPS_SERVER=smtp.gmail.com             # Gmail's SMTP server address
SMTPS_PORT=465                          # Port for SSL/TLS email encryption
SENDER_NAME=OpenFarm                    # Display name shown to email recipients
```

> **Important Note on Gmail App Password**: 
> 1. This is NOT your regular Gmail password
> 2. You need to have 2-factor authentication enabled on your Google account
> 3. Generate an App Password at: https://myaccount.google.com/security > App passwords

## Step 2: Build and Run the System
After setting up all necessary files, follow these steps to build and run the email ticket system:

Ensure all files are in place
```
project-root/
├── docker-compose.yml
├── .env
├── postgres/
│   ├── Dockerfile
│   └── init-db/        
│       ├── 01-init.sql
│       └── 02-init.sh    
└── email-sender/
    ├── Dockerfile
    ├── email-sender.c
    └── Makefile
```

## From the project root directory

### Stop any existing containers

```
docker-compose down
```

### Remove old volumes to ensure a clean state

```
docker volume rm $(docker volume ls -q | grep postgres_data) 2>/dev/null || true
```

### Build the containers

```
docker-compose build --no-cache
```

### Start the containers
```
docker-compose up    # starts containers in shell foreground

docker-compose up -d # starts containers in shell background
```

You should see the following output:

```
Running 3/3
 ✔ Network mail-server_ticket-network  Created
 ✔ Container ticket-db                 Created
 ✔ Container email-sender              Created
```

Furthermore, the database system should be ready to accept connections

## Basic Ticket Insertion

To insert a new ticket into the database:

```
docker exec -it ticket-db psql -U postgres -d ticketdb -c "INSERT INTO tickets (email, subject, body) VALUES ('recipient@example.com', 'Test Subject', 'This is a test email body.');"
```
