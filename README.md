**Section I. Functionality** <br />
  The program does not meet all required features, as the server currently does not concatenate all the files, and send them to the client for the final check. 
 However, the code to check when all server-side files are filled has been completed. The one required feature that does not work fully is the checksum in the server program. 
 It checks to make sure the sent checksum and calculated (server-side) checksum match. The code currently does nothing if they don’t match – this is where the server should re-request the corrupted packet with a sendto() call. 

**Section II. Extra Features** <br />
	One small extra feature was added to this project. Inside the client program, when main() is called, the starting time of the program is measured (start), and the rest of the program runs through. 
 At the end of the main() function, the check_time(start) function is called. Inside check_time(), the time is measured once more (end), and the difference of these variables is obtained (duration). 
 Duration is printed out to the terminal in milliseconds. This function simply measures how long it takes for the client and server programs to run and exchange files, working as a performance indicator. 

**Section III. Program Instructions** <br />
	This program was written in C++11 with Unix/Linux sockets and functions within a 64-bit Ubuntu VirtualBox VM. It uses the local loopback address (127.0.0.1) as the IPv4 address. 
 However, it was also tested on two physical Debian-based Linux machines in a limited manner (Linux Mint, Linux Kali) with their private IPv4 addresses rather than the loopback address, and the program functioned identically. 
 It should function properly on most Linux configurations. More specific directions on how to set up and run the code are available in Section IV. 

**Section IV. User Guide** <br />
This program contains just two files, c.cpp (the client file) and s.cpp (the server file). The server must be running first, and then the client can start running to send the data. 

To run the server, it first needs to be compiled: ```g++ -std=c++11 s.cpp -o s``` <br />
Now it can be run with the following command: ```./s``` <br />
Similarly, the client program needs to be compiled: ```g++ -std=c++11 c.cpp -o c``` <br />
It can be run with the same command: ```./c``` <br />
Both programs will communicate together with the data inside the .txt files. 

Assuming both programs are within the working directory (./), the client .txt files need to be stored here: ```./ctxt``` <br />
Before the programs are running, a dir for the server .txt files also needs to be created: ```./stxt``` <br />

This allows the client program to read from the .txt files, as it anticipates these files to be within the ctxt dir. Similarly, the server program is expecting to write to the files within the stxt dir. 
If the stxt dir does not exist, the server will never write any data. The client files stored in the ctxt directory must be named as follows:

1.txt, 2.txt, 3.txt, … 9.txt, 10.txt

This allows both programs to iterate through the files, use randomly generated numbers (RNG) to select a random file out of the pool, among other features. 

The client program will need the following file: ```./ip_port.txt``` <br />

Within the working directory. This .txt contains two lines. The first line should hold only the server IP address, and the second line should only hold the server port. 
For example, if using the local loop back, the entire contents of the file should look like so:
                                                                                                            ```127.0.0.1 <br />
                                                                                                            7777 <br />```
                                                                                                            
Furthermore, both the client and server store their own IP addresses and port numbers inside their respective files. For the server, the IP address and port are both set in the #define section just below the namespace declaration. 
Should a different server IP be warranted, it should be changed both in the #define section in s.cpp, as well as within ip_port.txt. The client IP address and port are both set within the set_header() function, below the struct declarations. 
This is the only location where the client IP needs to be modified, should it be required. 

*Note: The client IP and port number are both variables of the Header object, and their information is loaded onto the header buffer, so every packet sent contains this information. 
This format was originally intended to help the server know what destination the client sent from. This feature has since been deprecated, as it is not required, but the artifacting still remains.

The complete buffer, which contains the header data as well as the .txt file contents, is a c_str array of 1024 characters. The header data is ~36 characters on average, however including ample padding, the header would be 64 bytes. 
If required, the c_str buffer could be increased within both of the client and server files to include even more data per packet. The header contains the following data: Client IP (String – 16 bytes), Client Port (String – 4 bytes), 
Filename (String – 8 bytes), 4 bytes of padding, Total Lines (Int – 8 bytes), Current Line (Int – 8 bytes), Total File Line [Fline] (Int – 8 bytes), Checksum (Int – 8 bytes), Payload (~960 bytes):

```
0		       7		       15		        23		        31	
+------------------------------+---------+----------------+---------+
|        Client IP Address	   |  Cport  |	  Filename	  | Padding |         
+-------------+----------------+---------+-------+--------+---------+
|    Tlines   |      Cline     |      Fline      |     Checksum     |               
+-------------+----------------+-----------------+------------------+
|                  Payload [Remainder of 1024 Bytes]                |
+-------------------------------------------------------------------+ 
```


Where 'Cport' = Client Port, 'Tlines' = Total Lines, 'Cline' = Current Line, and 'Fline' = Total File Line. The header is packed into the buffer, and the file contents are appended afterwards. 
Because the buffer’s entire size is 1024, the header data isn’t directly constrained for space. This means various other header variables could be created, and packed into the header buffer, should they be desired. 
 
To test packet loss, iproute2 was utilized. If required, it can be downloaded in a Debian-based terminal with a command like so: ```sudo apt install iproute2``` <br />
After download, to test for packet loss, the following command can be run: ```sudo tc qdisc add dev lo root netem loss 5%``` <br />

In this case, the network is simulating 5% packet loss. In this instance, the network is simulating packet loss on the local loopback, hence ‘lo’ within this last command.
