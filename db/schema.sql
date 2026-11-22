-- Схема бази даних "PhotoStudio Manager" (SQLite 3)
-- Грошові суми зберігаються в копійках (INTEGER).
-- Дата й час - текст у форматі ISO 8601: 'YYYY-MM-DD' або 'YYYY-MM-DD HH:MM'.

CREATE TABLE IF NOT EXISTS users (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    login       TEXT    NOT NULL UNIQUE CHECK (length(trim(login)) BETWEEN 3 AND 32),
    full_name   TEXT    NOT NULL CHECK (length(trim(full_name)) BETWEEN 1 AND 100),
    role        TEXT    NOT NULL CHECK (role IN ('admin', 'manager')),
    pass_hash   TEXT    NOT NULL,
    salt        TEXT    NOT NULL,
    active      INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS clients (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    full_name   TEXT    NOT NULL CHECK (length(trim(full_name)) BETWEEN 2 AND 100),
    phone       TEXT    NOT NULL UNIQUE CHECK (length(phone) = 13),
    email       TEXT,
    notes       TEXT,
    created_at  TEXT    NOT NULL
);

CREATE TABLE IF NOT EXISTS halls (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT    NOT NULL UNIQUE CHECK (length(trim(name)) BETWEEN 1 AND 60),
    area            REAL    NOT NULL DEFAULT 0 CHECK (area >= 0),
    price_per_hour  INTEGER NOT NULL CHECK (price_per_hour > 0),
    description     TEXT,
    color           TEXT    NOT NULL DEFAULT '#4f7cff',
    active          INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS props (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT    NOT NULL CHECK (length(trim(name)) BETWEEN 1 AND 80),
    category        TEXT    NOT NULL,
    total_qty       INTEGER NOT NULL CHECK (total_qty >= 0),
    price_per_hour  INTEGER NOT NULL CHECK (price_per_hour >= 0),
    price_per_day   INTEGER NOT NULL CHECK (price_per_day >= 0),
    deposit         INTEGER NOT NULL DEFAULT 0 CHECK (deposit >= 0),
    condition       TEXT,
    active          INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS bookings (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    client_id   INTEGER NOT NULL REFERENCES clients(id) ON DELETE RESTRICT,
    hall_id     INTEGER NOT NULL REFERENCES halls(id) ON DELETE RESTRICT,
    start_at    TEXT    NOT NULL,
    end_at      TEXT    NOT NULL,
    status      TEXT    NOT NULL DEFAULT 'new'
                CHECK (status IN ('new', 'confirmed', 'completed', 'cancelled')),
    hall_cost   INTEGER NOT NULL DEFAULT 0 CHECK (hall_cost >= 0),
    props_cost  INTEGER NOT NULL DEFAULT 0 CHECK (props_cost >= 0),
    discount    INTEGER NOT NULL DEFAULT 0 CHECK (discount BETWEEN 0 AND 30),
    total       INTEGER NOT NULL DEFAULT 0 CHECK (total >= 0),
    prepaid     INTEGER NOT NULL DEFAULT 0 CHECK (prepaid >= 0),
    notes       TEXT,
    created_by  INTEGER REFERENCES users(id),
    created_at  TEXT    NOT NULL,
    CHECK (end_at > start_at)
);

-- реквізит, заброньований разом із залом (зв'язок "багато до багатьох")
CREATE TABLE IF NOT EXISTS booking_props (
    booking_id  INTEGER NOT NULL REFERENCES bookings(id) ON DELETE CASCADE,
    prop_id     INTEGER NOT NULL REFERENCES props(id) ON DELETE RESTRICT,
    qty         INTEGER NOT NULL CHECK (qty > 0),
    price       INTEGER NOT NULL CHECK (price >= 0),
    PRIMARY KEY (booking_id, prop_id)
);

-- оренда реквізиту "на винос"
CREATE TABLE IF NOT EXISTS rentals (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    client_id   INTEGER NOT NULL REFERENCES clients(id) ON DELETE RESTRICT,
    start_date  TEXT    NOT NULL,
    end_date    TEXT    NOT NULL,
    returned_at TEXT,
    status      TEXT    NOT NULL DEFAULT 'active'
                CHECK (status IN ('active', 'returned', 'cancelled')),
    total       INTEGER NOT NULL DEFAULT 0 CHECK (total >= 0),
    deposit     INTEGER NOT NULL DEFAULT 0 CHECK (deposit >= 0),
    fine        INTEGER NOT NULL DEFAULT 0 CHECK (fine >= 0),
    notes       TEXT,
    created_by  INTEGER REFERENCES users(id),
    created_at  TEXT    NOT NULL,
    CHECK (end_date >= start_date)
);

CREATE TABLE IF NOT EXISTS rental_items (
    rental_id     INTEGER NOT NULL REFERENCES rentals(id) ON DELETE CASCADE,
    prop_id       INTEGER NOT NULL REFERENCES props(id) ON DELETE RESTRICT,
    qty           INTEGER NOT NULL CHECK (qty > 0),
    price_per_day INTEGER NOT NULL CHECK (price_per_day >= 0),
    PRIMARY KEY (rental_id, prop_id)
);

-- індекси для пошуку накладок у розкладі та історії клієнта
CREATE INDEX IF NOT EXISTS idx_bookings_hall_time ON bookings(hall_id, start_at, end_at);
CREATE INDEX IF NOT EXISTS idx_bookings_client ON bookings(client_id);
CREATE INDEX IF NOT EXISTS idx_booking_props_prop ON booking_props(prop_id);
CREATE INDEX IF NOT EXISTS idx_rentals_client ON rentals(client_id);
CREATE INDEX IF NOT EXISTS idx_rental_items_prop ON rental_items(prop_id);
