//Client:
#include <vector>
#include <random>
#include <chrono>
#include <netdb.h>
#include <fstream>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <stdexcept>

using namespace std;
using namespace chrono;

#define catch_error(message) do { perror(message), exit(EXIT_FAILURE); } while (0)

struct addrinfo saddr; //struct for getaddrinfo();
struct addrinfo *receive_data; //struct for getaddrinfo();

struct Header {
  //Sent in packet:
  int current_line; //to tell server what line is being currently sent;
  int total_lines; //to tell server total number of lines;
  int checksum; //to verify packet contents are not corrupted;
  string file_name; //to tell server the file names;
  string client_ip; string client_port; //to tell server ip and port [DEPRECATED];
  vector<int> fline_count; //to tell server total lines in each file;
  
  //For caching, not sent in packet:
  vector<int> memory; //keep track of current location in each txt;
  vector<vector<string>> complete_file_cache; //keep track of all data;
};

//seting some header variables, including client IP and port:
void set_header(Header& h) {
  h.client_ip = "127.0.0.1"; //change if not using loopback address;
  h.client_port = "7777";
  h.memory.assign(10, 0); //holds 10 int's, one for each line number in each file;
  h.fline_count.assign(10, 0); //holds 10 int's, one for total # of lines for each file;
}

//to update the header, so sent packets have most recent data:
void get_header(Header& h, char* buff, int size, string& header_temp) {
  header_temp += h.client_ip + ";";
  header_temp += h.client_port + ";";
  header_temp += h.file_name + ".txt" + ";";
  header_temp += to_string(h.total_lines) + ";";
  header_temp += to_string(h.current_line) + ";";
  header_temp += to_string(h.fline_count[stoi(h.file_name) - 1]) + ";";
  header_temp += to_string(h.checksum) + ";";
  
  header_temp += "|"; //signifies the end of the header data, buffer data will follow;
  strncat(buff, header_temp.c_str(), size);
  header_temp.clear();
}

//calculates a checksum to send in the packet header:
void checksum(Header& h, char* buffer, string temp) {
  string buff = buffer;

  //checksum uses packet length, current line, and fline_count to generate unique number:
  int b = buff.length() + temp.length();
  int l = h.current_line + h.fline_count[stoi(h.file_name) - 1];
  
  h.checksum = (b * l);
}

//calculates how long entire service takes to run; Used for code optimization:
template<typename T>
void check_time(T start) {
  auto end = high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
  cout << "Execution time: " << duration.count() << " ms" << endl;
}

//setting up the connection between the client and the server:
void setup_sconnection(addrinfo& saddr, int csock) {
  string ip;
  string port;
  const char* s_IP;
  const char* s_PORT;
  ifstream s_in;
  
  s_in.open("ip_port.txt");
  getline(s_in, ip); //holds server IP addr;
  getline(s_in, port); //holds server PORT;
  s_in.close();
  
  s_IP = ip.c_str(); //setting server IP to a cstr;
  s_PORT = port.c_str(); //setting server port to a cstr;
  
  memset(&saddr, 0, sizeof(saddr));
  saddr.ai_family = AF_INET; //IPv4;
  saddr.ai_socktype = SOCK_DGRAM; //UDP;
  saddr.ai_flags = 0; //no flags specified;

  //grabbing IP, port, setting client socket to non-blocking to allow for sendto()/recvfrom() "multiplexing":
  getaddrinfo(s_IP, s_PORT, &saddr, &receive_data);
  fcntl(csock, F_SETFL, O_NONBLOCK);
}

//parsing received (lost data) packet with ; delimiter:
vector<string> parse(string input) {
  string token;
  string delim = ";";
  vector<string> tokens;
  size_t pos = 0;

  while ((pos = input.find(delim)) != string::npos) {
    token = input.substr(0, pos);
    tokens.push_back(token);
    input.erase(0, pos + delim.length());
  }

  tokens.push_back(input);
  return tokens;
}


//sending packets loaded with data, as well as handling (lost data) packets:
void send_data(Header& h, ifstream& d_in, string line, int rand,
	       string& temp, string& header_temp, char* buff, int size, int csock, socklen_t saddr_len) {

  int lost;
  char lost_data[1024];
  vector<string> lost_parsed;
  string file; string current_line; string lost_string;
  int counter;
  
  int index = (stoi(h.file_name) - 1); //file name, zero-indexed;

  ///
  int counting;
  ///
  
  //updates memory[] and current line as long as memory isn't trying to reach out of bounds:
  if (h.memory[index] <= h.complete_file_cache[index].size()) {
    h.memory[index]++;
    h.current_line = h.memory[index];
  } else { //resets memory and it's index once it reaches the limit:
    index = 0;
    h.memory.clear();
    cout << "reset index and memory" << endl;

    ///
    //counting++;
    //if (counting == 1) throw 0;
    ///
    
    return;
  }

  //ensuring current line does not exceed the end of the file cache:
  if (h.current_line > h.complete_file_cache[index].size()) {
    cout << "current_line too big" << endl;
    return;
  }

  //loading current line from file contents into a temp variable:
  temp = h.complete_file_cache[index][h.current_line - 1];

  //grabbing accurate header info (excluding checksum):
  get_header(h, buff, size, header_temp);
  checksum(h, buff, temp); //calculating checksum for packet header;
  memset(buff, 0, sizeof(buff)); //emptying packet buffer;
  
  get_header(h, buff, size, header_temp); //grabbing accurate header info (including checksum);
  
  strncat(buff, temp.c_str(), size); //concatting current line from file contents into buff variable;
  
  cout << "<-" << buff << endl;

  //send fully loaded packet to server:
  sendto(csock, buff, strlen(buff), 0, receive_data -> ai_addr, receive_data -> ai_addrlen);
  memset(buff, 0, sizeof(buff));
  temp.clear();
  h.checksum = 0;

  //listening, waiting to grab packets from server, these are the lost data packets:
  lost = recvfrom(csock, lost_data, sizeof(lost_data), 0, (struct sockaddr *) &saddr, &saddr_len);

  //when we receive lost packets:
  if (lost > 0) {
    cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;

    //parse out the packet:
    lost_string = string(lost_data, lost);
    memset(lost_data, 0, sizeof(lost_data));
    lost_parsed = parse(lost_string);

    //set the parsed data equal to variables:
    file = lost_parsed[0];
    current_line = lost_parsed[1];

    file.erase(file.length() - 4); //removing ".txt" from the end of the string;    
    h.file_name = file;
    h.current_line = stoi(current_line);
    h.memory[index] = h.current_line; //updating memory to most recent line;
    index = (stoi(h.file_name) - 1); //updating index to most recent file;

    cout << "-> file: " << h.file_name << ". current_line: " << h.current_line << endl;

    //loading current line from (lost data) packet into a temp variable:
    temp = h.complete_file_cache[index][h.current_line - 1];

    //grabbing accurate header info (excluding checksum):
    get_header(h, buff, size, header_temp);
    checksum(h, buff, temp);  //calculating checksum for packet header;
    memset(buff, 0, sizeof(buff)); //emptying packet buffer;
    get_header(h, buff, size, header_temp);  //grabbing accurate header info (including checksum);
    
    strncat(buff, temp.c_str(), size); //reloading fixed data back into buffer;
    
    cout << "<-" << buff << endl;

    //send requested packet back to server:
    sendto(csock, buff, strlen(buff), 0, receive_data -> ai_addr, receive_data -> ai_addrlen);
    memset(buff, 0, sizeof(buff));
    h.checksum = 0; //reset checksum;
     
    cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
  }
}
  

int main() {
  auto start = high_resolution_clock::now();
  
  int csock = socket(AF_INET, SOCK_DGRAM, 0); //client socket;
  
  Header h;
  ifstream d_in;
  char buff[1024];
  string temp;
  string buffer; string input_buffer;
  string header_temp; string line;
  size_t size = strlen(buff);
  ssize_t rec;
  socklen_t saddr_len = sizeof(saddr);
  vector<string> b_output;
  random_device rd;
  mt19937 gen(rd());
  int min_i = 1; int max_i = 10; int max_j = 3;
  int rand;
  
  vector<string> single_file_cache;

  h.current_line = 0;
  
  setup_sconnection(saddr, csock); //setting rules, making socket non-blocking;
  set_header(h); //initializing some of the header data;

  //gets line count for each file (fline), total line # for all files (total_lines):
  for (int i = 0; i < 10; i++) {
    d_in.open("ctxt/" + to_string(i + 1) + ".txt");

    while (getline(d_in, line)) {
      h.total_lines++;
      h.fline_count[i]++;

      single_file_cache.push_back(line); //temp vector to push data into cfc;
    }    

    h.complete_file_cache.push_back(single_file_cache); //holds all data from all files;
    single_file_cache.clear(); //clear temp vector for next itr;
    
    d_in.close(); //close and clear the instream;
    d_in.clear();
  }

  memset(buff, 0, sizeof(buff));
  
  while (true) {
    uniform_int_distribution<> dist_i(min_i, max_i); //calculates random number [1, 10];
    rand = dist_i(gen); 
    h.file_name = to_string(rand); //sets file to the RNG number;
    
    uniform_int_distribution<> dist_j(min_i, max_j); //calculates random number [1, 3];
    rand = dist_j(gen); //sets number of lines to the RNG number;

    //iterates through random file with random lines:
    for (int i = 0; i < rand; i++) {

      //try { ///
      //send_data(h, d_in, line, rand, temp, //counter, temp,
      //header_temp, buff, sizeof(buff), csock, saddr_len);
      //} catch (int) { ///
      //return 0; ///
      //} ///
    } 
    
    h.current_line = 0;
  }
  
  if (close(csock) == -1) {
    catch_error("[Close]");
  }

  check_time(start);
 
  return 0;
}
