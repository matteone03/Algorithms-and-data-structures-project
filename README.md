📦 Order Management System (Algorithms & Data Structures Project)
Overview

This project was developed as part of my university coursework on Data Structures and Algorithms.
The goal was to design and implement an order management system capable of handling large volumes of orders efficiently, while optimising the usage of stock and minimising waste.

The challenge was not only to make the system correct, but also time- and space-efficient, applying algorithmic thinking to real-world constraints.

Features

📑 Order ingestion: process incoming orders dynamically.

⚡ Efficient scheduling: prioritise deliveries based on stock availability and expiry dates.

🧮 Optimisation: minimise waste by reallocating stock whenever possible.

🛠 Low-level implementation in C: direct memory management and focus on performance.

Technical Details

Language: C

Core concepts:

- Custom data structures (linked lists, priority queues).

- Greedy and dynamic programming techniques.

- Complexity analysis to ensure scalability.

-->Result: designed algorithms that consistently reduced waste and achieved the top-performing solution in the course benchmark.

Example (pseudo-input/output)
Input orders:
- Order A: needs 10 units, expiry in 2 days
- Order B: needs 5 units, expiry in 1 day
- Order C: needs 7 units, expiry in 3 days

Output schedule:
- Fulfil Order B first (expiry soon)
- Fulfil Order A
- Allocate remaining stock to Order C

Lessons Learned

How to balance algorithmic complexity with practical system constraints.

The importance of low-level programming (manual memory management, pointers, efficiency in C).

Designing for fairness and reliability in a system where resources are limited.

How to Run
# Clone the repository
git clone https://github.com/matteone03/order-management-system.git
cd order-management-system

# Compile
gcc -o order_mgmt main.c

# Run
./order_mgmt input.txt
