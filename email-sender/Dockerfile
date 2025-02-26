FROM debian:bullseye-slim

# Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    libpq-dev \
    libcurl4-openssl-dev \
    make \
    postgresql-client \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy source code
WORKDIR /app
COPY email-sender.c .
COPY Makefile .

# Build the email sender
RUN make

# Run the email sender
CMD ["./email-sender"]
