Paged Memory System Implementation

BEFORE YOU REVIEW THIS CODE you should know that this project requires a good understanding of how Caching and Virtual Memory
System works.

Overview:
For this assignment, I used C to implement a Virtual Address to Physical Address
memory mapping system using bit-level operators.
I was requiered to build a paged memory management system with three components: TLB, Page Table,
and L1 Cache. The TLB was a 4-way Set Associative and your L1 Cache will be 2-way Set Associative.

I designed and implemented these three components in two required functions (see
caching.c) and any other functions that were necessary to create to support them. For your part of this program,
I was had to be able to receive a virtual address and output the associated byte in that virtual address.
I hada to design and implement the components using any data structures I considered apropriate (like structs and arrays).
The format of the virtual and physical addresses is as follows:
• Virtual address – 24 bits (18 bit VPN, 6 bit VPO)
• Physical address – 16 bits (10 bit PPN, 6 bit PPO)
