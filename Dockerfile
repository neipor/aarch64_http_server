# --- Build Stage ---
FROM gcc:latest AS builder

# Install dependencies
RUN apt-get update && apt-get install -y \
    make \
    openssl \
    libssl-dev

# Copy source code
WORKDIR /app
COPY . .

# Build the server
RUN make

# --- Final Stage ---
FROM debian:stable-slim

# Copy the built binary and necessary files from the builder stage
COPY --from=builder /app/anx /usr/local/bin/anx
COPY --from=builder /app/server.conf /etc/anx/server.conf
COPY --from=builder /app/www /var/www/anx
COPY --from=builder /app/certs /etc/anx/certs

# Create a non-root user to run the server
RUN useradd -ms /bin/bash anxuser
USER anxuser

# Expose ports (as specified in the default server.conf)
# Note: Docker maps these to high ports on the host, avoiding privilege issues.
EXPOSE 80
EXPOSE 443
EXPOSE 8080

WORKDIR /home/anxuser
# Set the default command to run the server
CMD ["anx", "/etc/anx/server.conf"] 