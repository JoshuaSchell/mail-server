/**
 * email_sender.c
 *
 * An automated system that monitors a PostgreSQL database for ticket insertions
 * and sends corresponding emails to recipients.
 *
 * This program connects to a PostgreSQL database, listens for notifications when new
 * tickets are inserted, and automatically sends emails for each new ticket. It also
 * updates the ticket status throughout the processing lifecycle.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <postgresql/libpq-fe.h>  /* PostgreSQL C client library */
#include <curl/curl.h>            /* libcurl for SMTP communication */
#include <unistd.h>
#include <regex.h>

/* Configuration constants */
#define MAX_EMAIL_SIZE 8192       /* Maximum size of email payload */
#define MAX_QUERY_SIZE 4096       /* Maximum size of SQL queries */

/* Environment variables for configuration */
char *DB_HOST;        /* PostgreSQL server hostname */
char *DB_PORT;        /* PostgreSQL server port */
char *DB_NAME;        /* PostgreSQL database name */
char *DB_USER;        /* PostgreSQL username */
char *DB_PASSWORD;    /* PostgreSQL password */
char *GMAIL_EMAIL;    /* Gmail sender address */
char *GMAIL_PASSWORD; /* Gmail app password */
char *SMTPS_SERVER;   /* SMTP server hostname */
char *SMTPS_PORT;     /* SMTP server port */
char *SENDER_NAME;    /* Display name for sender */

/**
 * Sanitizes a string for safe inclusion in SQL queries.
 *
 * @param conn  Active PostgreSQL connection
 * @param input String that needs to be escaped
 * @return      Escaped string safe for SQL (must be freed by caller)
 */
char* sanitize_sql_string(PGconn *conn, const char *input) {
    return PQescapeLiteral(conn, input, strlen(input));
}

/**
 * Validates an email address using regex pattern matching.
 *
 * @param email The email address to validate
 * @return      1 if email is valid, 0 if invalid
 */
int is_valid_email(const char *email) {
    regex_t regex;
    int reti;

    /* Compile the regex pattern for email validation */
    reti = regcomp(&regex, "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$", REG_EXTENDED);
    if (reti) {
        fprintf(stderr, "Could not compile regex\n");
        return 0;
    }

    /* Perform the regex match */
    reti = regexec(&regex, email, 0, NULL, 0);
    regfree(&regex);

    return (reti == 0); /* Return 1 if match, 0 if no match */
}

/**
 * Loads and validates all required environment variables.
 * Exits the program if critical variables are missing.
 */
void load_env_variables() {
    /* Database connection variables */
    DB_HOST = getenv("POSTGRES_HOST");
    DB_PORT = getenv("POSTGRES_PORT");
    DB_NAME = getenv("POSTGRES_DB");
    DB_USER = getenv("POSTGRES_USER");
    DB_PASSWORD = getenv("POSTGRES_PASSWORD");

    /* Email sending variables */
    GMAIL_EMAIL = getenv("GMAIL_EMAIL");
    GMAIL_PASSWORD = getenv("GMAIL_APP_PASSWORD");
    SMTPS_SERVER = getenv("SMTPS_SERVER");
    SMTPS_PORT = getenv("SMTPS_PORT");
    SENDER_NAME = getenv("SENDER_NAME");

    /* Use default sender name if not provided */
    if (!SENDER_NAME || strlen(SENDER_NAME) == 0) {
        SENDER_NAME = "OpenFarm"; // Default sender name
    }

    /* Validate required environment variables */
    if (!DB_HOST) {
        fprintf(stderr, "Error: Missing POSTGRES_HOST environment variable\n");
        exit(1);
    }
    if (!DB_PORT) {
        fprintf(stderr, "Error: Missing POSTGRES_PORT environment variable\n");
        exit(1);
    }
    if (!DB_NAME) {
        fprintf(stderr, "Error: Missing POSTGRES_DB environment variable\n");
        exit(1);
    }
    if (!DB_USER) {
        fprintf(stderr, "Error: Missing POSTGRES_USER environment variable\n");
        exit(1);
    }
    if (!DB_PASSWORD) {
        fprintf(stderr, "Error: Missing POSTGRES_PASSWORD environment variable\n");
        exit(1);
    }
    if (!GMAIL_EMAIL) {
        fprintf(stderr, "Error: Missing GMAIL_EMAIL environment variable\n");
        exit(1);
    }
    if (!GMAIL_PASSWORD) {
        fprintf(stderr, "Error: Missing GMAIL_APP_PASSWORD environment variable\n");
        exit(1);
    }
    if (!SMTPS_SERVER) {
        fprintf(stderr, "Error: Missing SMTP_SERVER environment variable\n");
        exit(1);
    }
    if (!SMTPS_PORT) {
        fprintf(stderr, "Error: Missing SMTP_PORT environment variable\n");
        exit(1);
    }

    /* Log successful loading of environment variables */
    printf("Environment variables loaded successfully\n");
    printf("Database: %s:%s/%s\n", DB_HOST, DB_PORT, DB_NAME);
    printf("SMTP: %s:%s\n", SMTPS_SERVER, SMTPS_PORT);
    printf("Email: %s\n", GMAIL_EMAIL);
    printf("Sender Name: %s\n", SENDER_NAME);
}

/**
 * Establishes a connection to the PostgreSQL database.
 *
 * @return Active PostgreSQL connection or NULL if connection failed
 */
PGconn* connect_to_db() {
    char conninfo[256];

    /* Build the connection string */
    sprintf(conninfo, "host=%s port=%s dbname=%s user=%s password=%s",
            DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD);

    /* Attempt to connect to the database */
    PGconn *conn = PQconnectdb(conninfo);

    /* Check if connection was successful */
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Connection to database failed: %s", PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }

    return conn;
}

/**
 * Sends an email using the SMTP protocol via libcurl.
 *
 * @param to      Recipient email address
 * @param subject Email subject line
 * @param body    Email body content
 * @return        1 if email was sent successfully, 0 if failed
 */
int send_email(const char *to, const char *subject, const char *body) {
    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;
    char url[100];

    /* Build the SMTP URL with TLS/SSL */
    snprintf(url, sizeof(url), "smtps://%s:%s", SMTPS_SERVER, SMTPS_PORT);

    /* Initialize curl */
    curl = curl_easy_init();
    if (curl) {
        char payload[MAX_EMAIL_SIZE];
        char from_header[256];
        char to_header[256];
        char subject_header[512];

        /* Prepare email headers */
        snprintf(from_header, sizeof(from_header), "From: %s <%s>", SENDER_NAME, GMAIL_EMAIL);
        snprintf(to_header, sizeof(to_header), "To: <%s>", to);
        snprintf(subject_header, sizeof(subject_header), "Subject: %s", subject);

        /* Create the complete email payload with headers and body */
        snprintf(payload, sizeof(payload),
                 "%s\r\n%s\r\n%s\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\n%s\r\n",
                 from_header, to_header, subject_header, body);

        /* Configure curl for SMTP communication */
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_USERNAME, GMAIL_EMAIL);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, GMAIL_PASSWORD);

        /* Configure TLS/SSL security settings */
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        /* Set the sender and recipient addresses */
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, GMAIL_EMAIL);
        recipients = curl_slist_append(recipients, to);
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        /* Configure the email data upload */
        curl_easy_setopt(curl, CURLOPT_READDATA, payload);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION,
                         (size_t (*)(char *, size_t, size_t, void *))fread);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        /* Create a memory stream for the payload */
        FILE *fd = fmemopen(payload, strlen(payload), "rb");
        curl_easy_setopt(curl, CURLOPT_READDATA, fd);

        /* Perform the email sending operation */
        res = curl_easy_perform(curl);

        /* Clean up resources */
        fclose(fd);
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);

        /* Check if sending succeeded */
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return 0;
        }
    }

    return (res == CURLE_OK) ? 1 : 0;
}

/**
 * Processes a ticket by sending an email and updating its status.
 *
 * @param conn      Active PostgreSQL connection
 * @param ticket_id ID of the ticket to process
 */
void process_ticket(PGconn *conn, int ticket_id) {
    char query[MAX_QUERY_SIZE];
    PGresult *res;

    /* Authentication failure counter for rate limiting */
    static int auth_failures = 0;
    const int MAX_AUTH_FAILURES = 5;

    /* Rate limiting for repeated authentication failures */
    if (auth_failures >= MAX_AUTH_FAILURES) {
        fprintf(stderr, "Too many authentication failures, waiting 15 minutes before trying again\n");
        sleep(900); /* Wait 15 minutes */
        auth_failures = 0; /* Reset counter after waiting */
    }

    /* Update ticket status to 'processing' */
    snprintf(query, sizeof(query),
             "UPDATE tickets SET status = 'processing' WHERE id = %d AND status = 'received'",
             ticket_id);

    res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Failed to update ticket to processing: %s", PQerrorMessage(conn));
        PQclear(res);
        return;
    }
    PQclear(res);

    /* Retrieve ticket information */
    snprintf(query, sizeof(query),
             "SELECT email, subject, body FROM tickets WHERE id = %d AND status = 'processing'",
             ticket_id);

    res = PQexec(conn, query);

    /* Check if ticket exists and is in 'processing' state */
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        fprintf(stderr, "No processing ticket found with ID %d\n", ticket_id);
        PQclear(res);
        return;
    }

    /* Extract ticket data from query result */
    char *email = PQgetvalue(res, 0, 0);
    char *subject = PQgetvalue(res, 0, 1);
    char *body = PQgetvalue(res, 0, 2);

    printf("Sending email to: %s\nSubject: %s\nBody: %s\n", email, subject, body);

    /* Validate email format before sending */
    if (!is_valid_email(email)) {
        fprintf(stderr, "Invalid email format: %s\n", email);
        PQclear(res);

        /* Update status to indicate validation error */
        snprintf(query, sizeof(query),
                "UPDATE tickets SET status = 'processing', sent_at = NOW() WHERE id = %d",
                ticket_id);
        res = PQexec(conn, query);
        PQclear(res);
        return;
    }

    /* Attempt to send the email */
    if (send_email(email, subject, body)) {
        printf("Email sent successfully to %s\n", email);

        /* Update ticket status to 'completed' and record sent timestamp */
        PQclear(res);
        snprintf(query, sizeof(query),
                 "UPDATE tickets SET sent_at = NOW(), status = 'completed' WHERE id = %d",
                 ticket_id);

        res = PQexec(conn, query);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            fprintf(stderr, "Failed to update ticket status: %s", PQerrorMessage(conn));
        }

        /* Reset auth failures counter on success */
        auth_failures = 0;
    } else {
        /* Handle email sending failure */
        fprintf(stderr, "Failed to send email to %s, keeping status as processing\n", email);
        auth_failures++;
        fprintf(stderr, "Email sending failure detected (%d/%d)\n",
                auth_failures, MAX_AUTH_FAILURES);
    }

    PQclear(res);
}

/**
 * Main function: initializes systems, connects to database, and
 * processes tickets in an infinite loop.
 */
int main() {
    /* Initialize environment and configurations */
    load_env_variables();

    /* Initialize curl library */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* Connect to PostgreSQL database */
    PGconn *conn = connect_to_db();
    if (!conn) {
        return 1;
    }

    /* Set up notification listening */
    PGresult *res = PQexec(conn, "LISTEN new_ticket");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "LISTEN command failed: %s", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        return 1;
    }
    PQclear(res);

    printf("Email sender started. Waiting for new tickets...\n");

    /* Process any existing tickets in received or processing state */
    res = PQexec(conn, "SELECT id FROM tickets WHERE status IN ('received', 'processing')");
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++) {
            int ticket_id = atoi(PQgetvalue(res, i, 0));
            process_ticket(conn, ticket_id);
        }
    }
    PQclear(res);

    /* Main event loop: wait for and process notifications */
    while (1) {
        PGnotify *notify;

        /* Check for notifications */
        PQconsumeInput(conn);

        /* Process any received notifications */
        while ((notify = PQnotifies(conn)) != NULL) {
            printf("Received notification for ticket ID: %s\n", notify->extra);
            int ticket_id = atoi(notify->extra);
            process_ticket(conn, ticket_id);
            PQfreemem(notify);
        }

        /* Sleep to avoid excessive CPU usage
         * Check every second */
        sleep(1);
    }

    /* Cleanup resources (never reached in current implementation) */
    PQfinish(conn);
    curl_global_cleanup();

    return 0;
}
