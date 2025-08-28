//Server:
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <fstream>
#include <vector>

using namespace std;

#define catch_error(message) do { perror(message), exit(EXIT_FAILURE); } while (0)
#define S_IP "127.0.0.1" //setting own IP, update if not using loopback addr;
#define S_PORT "7777" //setting own port.

struct addrinfo caddr; //struct for getaddrinfo();
struct addrinfo *receive_data; //struct for getaddrinfo();

//Header holds 7 fields for client/server to know what data is being sent/received:
struct Header { 
  int current_line; 
  int total_lines;
  int fline_count;
  int checksum;
  string file_name;
  string client_ip; string client_port; //[DEPRECATED];
};

//calculates a checksum to compare to the one sent in the packet header:
void checksum(Header& h, string buffer) { 
  string str_check = to_string(h.checksum);
  string str_temp = "0"; //In client, when checksum length is first calculated, h.checksum is set to 0;

  //calculating the difference in the h.checksum value, before and after h.checksum is updated:
  int difference = str_check.length() - str_temp.length(); 
  int calc_length = buffer.length() - difference;  
  int check = calc_length * (h.current_line + h.fline_count); //to compare with received h.checksum;

  //making sure there is no data loss/corruption:
  if (check != h.checksum) {

    //here is where to re-request the corrupted packet using sendto();
  }
}

//setting up the connection between the server and client:
void setup_connection(int ssock, addrinfo& caddr) {
  int binding; int getting_info;
  
  memset(&caddr, 0, sizeof(caddr));
  caddr.ai_family = AF_INET; //IPv4;
  caddr.ai_socktype = SOCK_DGRAM; //UDP;
  caddr.ai_flags = AI_PASSIVE; //to connect with bind();

  //grabbing IP, port, and binding to server socket:
  getting_info = getaddrinfo(S_IP, S_PORT, &caddr, &receive_data); 
  binding = bind(ssock, receive_data -> ai_addr, receive_data -> ai_addrlen);

  //basic error checking:
  if (getting_info != 0) {
    catch_error("[Getaddrinfo]");
  } else if (ssock == -1) {
    catch_error("[Socket]");
  } else if (binding == -1) {
    catch_error("[Bind]");
  }
}

//parsing received (standard) packet with a delimiter:
vector<string> parse(string input, string delim) {
  string token;
  vector<string> tokens;
  size_t pos = 0;

  while ((pos = input.find(delim)) != string::npos) {
    token = input.substr(0, pos);
    tokens.push_back(token);
    input.erase(0, pos + delim.length());

    //exit function upon finding the first "|" delimiter. This allows the .txt contents to still contain
    //pipes without it being treated as a delimiter:
    if (delim == "|") {
      break;
    }
  }

  tokens.push_back(input);
  return tokens;
}

int main() {
  
  int ssock = socket(AF_INET, SOCK_DGRAM, 0); //initializing socket;

  Header h;
  int inc;
  char input_buff[1024];
  string line; string buff_temp;
  string concat_temp; string input_buffer;
  string b_delim; string h_delim;
  ssize_t rec;
  socklen_t caddr_len = sizeof(caddr);
  const char* c_IP; const char* c_PORT;
  vector<string> b_output; vector<string> h_output;

  vector<bool> full_file;
  int full_file_itr;
  full_file.assign(10, false);
  
  setup_connection(ssock, caddr); //setting rules, binding to the server socket;

  inc = 0;
  b_delim = "|"; h_delim = ";"; //| and ; are to be used as delimiters in the parse() function;

  //looping through to create outstreams for all .txt files: 
  for (int i = 1; i < 11; i++) {
    ofstream out("stxt/" + to_string(i) + ".txt");
  }
  
  while (true) {
    //receiving packet from client:
    rec = recvfrom(ssock, input_buff, sizeof(input_buff) - 1, 0, (struct sockaddr*) &caddr, &caddr_len);
    buff_temp = string(input_buff, rec);
    memset(input_buff, 0, sizeof(input_buff));

    cout << "<-" << buff_temp << endl;

    //splitting packet in half; b_output[0] is the header data, b_output[1] is the .txt contents: 
    b_output = parse(buff_temp, b_delim); //parses with |;

    //splitting header into all header data:
    h_output = parse(b_output[0], h_delim); //parses with ;;
    h_output[7] = b_output[1];
    b_output.clear();

    //setting all parsed header data as struct Header objects:
    h.client_ip = h_output[0];
    h.client_port = h_output[1];
    h.file_name = h_output[2];
    h.total_lines = stoi(h_output[3]);
    h.current_line = stoi(h_output[4]);
    h.fline_count = stoi(h_output[5]);
    h.checksum = stoi(h_output[6]);
    input_buffer = h_output[7];
    h_output.clear();

    //calculating checksum with received packet, comparing with checksum sent in header:
    checksum(h, buff_temp);
    buff_temp.clear();
    
    //    c_IP = (h.client_ip).c_str();
    //c_PORT = (h.client_port).c_str();
    
    int next = 0;
    string line; string missing_data;
    ifstream in("stxt/" + h.file_name);
    ofstream out;

    //next calculates how many lines have been gathered and stored thus far in the .txt files:
    while(getline(in, line)) {
      next++;
    }

    in.close();

    //ensuring next + 1 (the value we are receiving next) does not surpass total file line count:
    //if ((next + 1) > h.fline_count) {
    //  continue;
    //}

    int file_temp = stoi(h.file_name) - 1;
    
    if (next + 1 == h.fline_count) {
      full_file[file_temp] = true;
      cout << h.file_name << "'s file full!" << endl;

      for (auto i : full_file) {
	if (i) {
	  full_file_itr++;

	  if (full_file_itr == 10) {
	    //all files have matched their flines, this is where the files are concatenated and sent back
	    //to the server as one big .txt file using sendto();
	  }
	}
      }

      full_file_itr = 0;
    }
    
    //only writing to file if matching with the current line:
    if (h.current_line == next + 1) {
      out.open("stxt/" + h.file_name, ios_base::app); //appending to file contents;
      out << input_buffer << "\n"; //writing each line into it's appropriate file;
      out.close();
    } else { //packet loss correction:
      cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
      cout << "We received: " << h.file_name << ", " << h.current_line;
      cout << ". We expected: " << next + 1 << endl;

      //Gathering data for packet that we want, sending to client with custom missing_data header:
      missing_data = h.file_name + ";" + to_string(next + 1);
      cout << "sending: " << missing_data << " -> " << endl;
      sendto(ssock, missing_data.c_str(), strlen(missing_data.c_str()), 0, (struct sockaddr *) &caddr, caddr_len);
      cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
    }
  }
  
  //basic error checking:
  if (close(ssock) == -1) {
    catch_error("[Close]");
  }
  
  return 0;
}
