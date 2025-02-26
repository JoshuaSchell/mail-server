-- Create enum type for ticket status
CREATE TYPE ticket_status AS ENUM ('received', 'processing', 'completed');

-- Create the tickets table
CREATE TABLE tickets (
    id SERIAL PRIMARY KEY,
    email VARCHAR(255) NOT NULL CHECK (email ~* '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}'),
    status ticket_status NOT NULL DEFAULT 'received',
    subject VARCHAR(255) NOT NULL CHECK (length(subject) > 0),
    body TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    sent_at TIMESTAMP,
    retry_count INTEGER DEFAULT 0,
    last_error TEXT
);

-- Create index for status for faster lookups
CREATE INDEX idx_tickets_status ON tickets(status);

-- Create notification function
CREATE OR REPLACE FUNCTION notify_ticket_insertion()
RETURNS TRIGGER AS $$
BEGIN
    PERFORM pg_notify('new_ticket', NEW.id::TEXT);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Create trigger for the tickets table
CREATE TRIGGER ticket_inserted
AFTER INSERT ON tickets
FOR EACH ROW
EXECUTE FUNCTION notify_ticket_insertion();

-- Add a function to clean old completed tickets after 30 days
CREATE OR REPLACE FUNCTION clean_old_tickets()
RETURNS VOID AS $$
BEGIN
    DELETE FROM tickets
    WHERE status = 'completed'
    AND sent_at < (CURRENT_TIMESTAMP - INTERVAL '30 days');
END;
$$ LANGUAGE plpgsql;
