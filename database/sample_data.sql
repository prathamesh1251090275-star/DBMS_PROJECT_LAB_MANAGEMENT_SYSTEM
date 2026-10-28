-- Sample data. Loaded only when the database is created for the first time.
-- Dates use offsets from today so overdue / due-soon examples always make sense.
-- No passwords are stored: run  lab_server --set-password <username>  to enable login.
PRAGMA foreign_keys = ON;

INSERT INTO Labs (lab_name, location, description) VALUES
 ('Electronics Lab', 'Block A, Room 101', 'Analog and digital electronics experiments'),
 ('Embedded Systems Lab', 'Block A, Room 204', 'Microcontroller and IoT projects'),
 ('Computer Lab', 'Block B, Room 305', 'Programming and networking practicals'),
 ('Physics Lab', 'Block C, Room 110', 'Optics and measurement experiments');

INSERT INTO Categories (category_name, description) VALUES
 ('Measuring Instruments', 'Devices that measure electrical or physical quantities'),
 ('Development Boards', 'Microcontroller and single-board computers'),
 ('Hand Tools', 'Soldering and assembly tools'),
 ('Computing Devices', 'Desktops and laptops'),
 ('Optical Instruments', 'Microscopes and lenses');

INSERT INTO Users (full_name, department, role, email, phone, username) VALUES
 ('Dr. Meera Kulkarni', 'Administration',   'Admin',        'meera.k@college.edu', '9800000001', 'admin'),
 ('Rahul Patil',        'Electronics',      'LabAssistant', 'rahul.p@college.edu', '9800000002', 'rahul.assist'),
 ('Sneha Joshi',        'Computer Science', 'LabAssistant', 'sneha.j@college.edu', '9800000003', 'sneha.assist'),
 ('Aarav Sharma',       'Computer Science', 'Student',      'aarav.s@college.edu', '9800000004', 'aarav'),
 ('Priya Deshmukh',     'Electronics',      'Student',      'priya.d@college.edu', '9800000005', 'priya'),
 ('Rohan Kale',         'Electronics',      'Student',      'rohan.k@college.edu', '9800000006', 'rohan'),
 ('Prof. Anil Gupta',   'Computer Science', 'Faculty',      'anil.g@college.edu',  '9800000007', 'anil'),
 ('Prof. Neha Rao',     'Physics',          'Faculty',      'neha.r@college.edu',  '9800000008', 'neha');

-- available = total - outstanding issued quantity - quantity under open maintenance
INSERT INTO Equipment (equipment_name, category_id, lab_id, total_quantity, available_quantity, condition_status, purchase_date, description) VALUES
 ('Digital Multimeter',  1, 1, 20, 18, 'Good', '2023-06-15', 'Handheld 6000-count multimeter'),
 ('Oscilloscope 100MHz', 1, 1,  8,  7, 'Good', '2022-08-01', 'Dual channel digital oscilloscope'),
 ('Function Generator',  1, 1,  6,  6, 'Good', '2022-08-01', '1 Hz - 10 MHz'),
 ('Arduino Uno Kit',     2, 2, 15, 13, 'Good', '2024-01-20', 'Arduino Uno R3 with sensors'),
 ('Raspberry Pi 4',      2, 2, 10, 10, 'Good', '2024-03-10', '4 GB RAM model'),
 ('Soldering Station',   3, 1, 12, 11, 'Fair', '2021-11-05', 'Temperature controlled'),
 ('Desktop Computer',    4, 3, 30, 25, 'Good', '2023-02-14', 'Core i5, 16 GB RAM'),
 ('Compound Microscope', 5, 4, 10, 10, 'Good', '2020-07-19', 'Binocular, 1000x'),
 ('Vernier Caliper',     1, 4, 25, 21, 'Good', '2021-04-02', '0.02 mm least count');

INSERT INTO Issue_Records (user_id, equipment_id, issued_by, quantity_issued, issue_date, due_date, status) VALUES
 (4, 1, 2, 3, date('now','localtime','-17 days'), date('now','localtime','-10 days'), 'Partially Returned'), -- 2 outstanding, overdue
 (5, 4, 2, 2, date('now','localtime','-3 days'),  date('now','localtime','+4 days'),  'Issued'),             -- on time
 (6, 2, 3, 1, date('now','localtime','-20 days'), date('now','localtime','-13 days'), 'Returned'),           -- fully returned
 (7, 7, 3, 5, date('now','localtime','-12 days'), date('now','localtime','-5 days'),  'Issued'),             -- overdue
 (4, 9, 2, 4, date('now','localtime','-1 days'),  date('now','localtime','+6 days'),  'Issued');             -- on time

INSERT INTO Return_Records (issue_id, quantity_returned, return_date, received_by, remarks) VALUES
 (1, 1, date('now','localtime','-12 days'), 2, 'Partial return, 2 still with student'),
 (3, 1, date('now','localtime','-14 days'), 3, 'Returned in good condition');

INSERT INTO Maintenance_Records (equipment_id, quantity_affected, problem_description, reported_by, reported_date, maintenance_date, completed_date, repair_cost_paise, status) VALUES
 (2, 1, 'Probe connector damaged',     2, date('now','localtime','-6 days'),  date('now','localtime','-4 days'),  NULL, 0, 'In Progress'),
 (1, 2, 'Display cracked',             2, date('now','localtime','-40 days'), date('now','localtime','-38 days'), date('now','localtime','-30 days'), 150000, 'Completed'),
 (6, 1, 'Heating element not working', 3, date('now','localtime','-1 days'),  NULL, NULL, 0, 'Reported'),
 (8, 1, 'Lens cracked beyond repair',  3, date('now','localtime','-60 days'), NULL, date('now','localtime','-55 days'), 0, 'Written Off');
