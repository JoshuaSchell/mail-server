FROM postgres:15

# Environment variables will be provided from docker-compose
# No default values here to ensure we use the ones from .env

# Copy initialization scripts
COPY init-db/ /docker-entrypoint-initdb.d/

# Make sure shell script is executable
RUN chmod +x /docker-entrypoint-initdb.d/02-init.sh

# Health check to verify the database is ready
HEALTHCHECK --interval=5s --timeout=5s --retries=5 \
    CMD pg_isready -U $POSTGRES_USER -d $POSTGRES_DB || exit 1

# Expose the PostgreSQL port
EXPOSE 5432
