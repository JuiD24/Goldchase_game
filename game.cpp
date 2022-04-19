#include "goldchase.h"
#include "Map.h"

#include <iostream>
#include <fstream>
#include <string>
#include <bitset>

#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <unistd.h>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include <semaphore.h>  /* semaphore */
#include <memory> /* shared ptr */

using namespace std;

struct goldMine {
  unsigned short rows;
  unsigned short cols;
  unsigned char players;
  unsigned char map[0];
};

int main(int argc, char *argv[]){
    unsigned char current_player;
    int player_position;
    unique_ptr<Map> pointer_to_rendering_map;
    srand (time(NULL));
    goldMine* gmp;
    sem_t* mysemaphore=nullptr;
    int shared_mem_fd;

    if(argc == 2){

      current_player=G_PLR0; //Assigning 1st player to current_player
        
      //Creating semaphore
      mysemaphore=sem_open("/goldchase_semaphore", O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR, 1);
      if(mysemaphore == SEM_FAILED){
        if(errno==EEXIST)
          cerr << "The game is already running. Do not provide a map file.\n";
        else //any other kind of semaphore open failure
          perror("Error in Creating & opening a semaphore");
        exit(1);
      }

      int syscall_result=sem_wait(mysemaphore); //"grab" a semaphore
      if(syscall_result==-1){ 
        perror("Failed sem_wait"); 
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        int semUnlinkError = sem_unlink("/goldchase_semaphore"); 
        if(semUnlinkError == -1){
           perror("ERROR unlinking semaphore");
        }
        return 2; 
      }

      ifstream mapfile(argv[1]);  //Read map file
      // If unable to open:  close & remove semaphore, and exit  
      if(! mapfile){
        cerr <<"Error opening map file";
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        int semUnlinkError = sem_unlink("/goldchase_semaphore"); 
        if(semUnlinkError == -1){
           perror("ERROR unlinking semaphore");
        }
        exit(1);
      }

      //read rows and column
      int cols=0; 
      int rows=0; 
      string line;
      getline(mapfile,line);
      int num_gold=stoi(line);
      int foolGold = num_gold - 1;
      while(getline(mapfile, line))
      {
      ++rows;
      cols=line.length()>cols ? line.length() : cols;
      }
      mapfile.close();
      
      //shared Memory and ftruncate
      shared_mem_fd=shm_open("/goldmemory", O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR); 

      if(shared_mem_fd == -1){
        cerr << "Player1 has already started. Please start again as Player 2.";
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        int semUnlinkError = sem_unlink("/goldchase_semaphore");
        if(semUnlinkError == -1){
           perror("ERROR unlinking semaphore");
        }
        exit(1);
      }

      int ftrunc = ftruncate(shared_mem_fd,sizeof(goldMine)+rows*cols);
      if(ftrunc == -1){
        cerr << "Error truncating the shared memory";
        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int shmUnlinkError = shm_unlink("/goldmemory");
        if(shmUnlinkError == -1){
          perror("ERROR unlinking shared memory");
        }
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        int semUnlinkError = sem_unlink("/goldchase_semaphore");
        if(semUnlinkError == -1){
           perror("ERROR unlinking semaphore");
        }
        exit(1);
      }
      
      //Allocate shared Memory using mmap
      void* ret = mmap(nullptr, sizeof(goldMine)+rows*cols, PROT_READ|PROT_WRITE, MAP_SHARED, shared_mem_fd, 0);

      if(ret == MAP_FAILED){
        cerr << "Error on mmap\n";
        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int shmUnlinkError = shm_unlink("/goldmemory");
        if(shmUnlinkError == -1){
          perror("ERROR unlinking shared memory");
        }
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        int semUnlinkError = sem_unlink("/goldchase_semaphore");
        if(semUnlinkError == -1){
           perror("ERROR unlinking semaphore");
        }
        exit(1);
      }
      
      gmp=(goldMine*)ret;
      //now initialize this memory with the number of rows and cols in the map;
      gmp->rows=rows;
      gmp->cols=cols;

      //Read map file to create map
      mapfile.open(argv[1]);
      // If unable to open:  close & remove semaphore, and exit  
      if(! mapfile){
        cerr <<"Error opening map file";
        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int shmUnlinkError = shm_unlink("/goldmemory");
        if(shmUnlinkError == -1){
          perror("ERROR unlinking shared memory");
        }
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        int semUnlinkError = sem_unlink("/goldchase_semaphore");
        if(semUnlinkError == -1){
           perror("ERROR unlinking semaphore");
        }
        exit(1);
      }

      getline(mapfile, line); //discard num gold
      int current_byte=0;
      while(getline(mapfile, line)) //read mapfile line by line
      {
      for(int i=0; i<line.length(); ++i)
      { 
        if(line[i] != '*' && line[i] != ' '){
          cerr << "Invalid characters in map text file.\n";
          int closeError = close(shared_mem_fd);
          if(closeError == -1){
            perror("ERROR closing shared memory");
          }
          int shmUnlinkError = shm_unlink("/goldmemory");
          if(shmUnlinkError == -1){
            perror("ERROR unlinking shared memory");
          }
          int semCloseError = sem_close(mysemaphore); 
          if(semCloseError == -1){
            perror("ERROR closing semaphore");
          }
          int semUnlinkError = sem_unlink("/goldchase_semaphore");
          if(semUnlinkError == -1){
            perror("ERROR unlinking semaphore");
          }
          exit(1);
        }

        if(line[i]=='*')  //If equal to '*' then put wall into map
          gmp->map[current_byte]=G_WALL;
        ++current_byte;
      }
      current_byte+=cols-line.length();
      }
      mapfile.close();

      //Find Random position for player 1
      player_position = (rand() % (rows*cols));

      while(gmp->map[player_position] != 0){
        player_position = (rand() % (rows*cols));
      }
      gmp->map[player_position] = G_PLR0; //Place payer 1
      gmp->players|=G_PLR0; //Turn on the Player 1 bit
      

      while(num_gold > 0){
        int gold_pos = (rand() % (rows*cols));
        while(gmp->map[gold_pos] != 0){
          gold_pos = (rand() % (rows*cols));
        }
        
        if(foolGold > 0){
          gmp->map[gold_pos] = G_FOOL;
          foolGold -= 1;
        }else{
          gmp->map[gold_pos] = G_GOLD;
        }
          num_gold -= 1;
      }
      syscall_result = sem_post(mysemaphore); // release the semaphore
      if(syscall_result==-1) { perror("Failed sem_post"); 
          int closeError = close(shared_mem_fd);
          if(closeError == -1){
            perror("ERROR closing shared memory");
          }
          int shmUnlinkError = shm_unlink("/goldmemory");
          if(shmUnlinkError == -1){
            perror("ERROR unlinking shared memory");
          }
          int semCloseError = sem_close(mysemaphore); 
          if(semCloseError == -1){
            perror("ERROR closing semaphore");
          }
          int semUnlinkError = sem_unlink("/goldchase_semaphore");
          if(semUnlinkError == -1){
            perror("ERROR unlinking semaphore");
          }
          exit(1); }

      try {
        pointer_to_rendering_map= make_unique<Map>(gmp->map, rows, cols);
      }catch(const std::exception& e) {
        cerr << e.what() << '\n';
        // if no players remain, close and delete
        // shared memory and semaphore
        int syscall_result=sem_wait(mysemaphore); //"grab" or "take" a semaphore
        if(syscall_result==-1) { 
          perror("Failed sem_wait"); 
          int closeError = close(shared_mem_fd);
          if(closeError == -1){
            perror("ERROR closing shared memory");
          }
          int semCloseError = sem_close(mysemaphore); 
          if(semCloseError == -1){
            perror("ERROR closing semaphore");
          }
          return 2; 
        }

        gmp->map[player_position] = 0;
        gmp->players &= ~current_player;

        syscall_result = sem_post(mysemaphore); // release the semaphore
        if(syscall_result==-1) { 
        perror("Failed sem_post"); 
        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        return 2; 
        }

        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }

        if(gmp->players == 0)
        {
          int shmUnlinkError = shm_unlink("/goldmemory"); 
          if(shmUnlinkError == -1){
            perror("ERROR unlinking shared memory");
          }
          int semUnlinkError = sem_unlink("/goldchase_semaphore"); 
          if(semUnlinkError == -1){
            perror("ERROR unlinking semaphore");
          }
        }
        exit(1);
      } 

    }else if(argc == 1){

      //open semaphore
      mysemaphore=sem_open("/goldchase_semaphore", O_RDWR); 
      if(mysemaphore == SEM_FAILED){
        if(errno==ENOENT)
          perror("Nobody is playing! Provide a map file to start the game.");
        else //any other kind of semaphore open failure
          perror("Open semaphore");
        exit(1);
      }
    
      int syscall_result=sem_wait(mysemaphore); //"grab" or "take" a semaphore
      if(syscall_result==-1) { perror("Failed sem_wait"); 
      int semCloseError = sem_close(mysemaphore); 
      if(semCloseError == -1){
        perror("ERROR closing semaphore");
      }
      return 2; 
      }
    
      //for subsequent players 
      shared_mem_fd=shm_open("/goldmemory", O_RDWR, 0); 
      if(shared_mem_fd == -1){
        cerr << "Nobody is playing! Provide a map file to start the game.\n";
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        exit(1);
      }

      unsigned short rows;
      unsigned short cols;
      int readRowReturn = read(shared_mem_fd, &rows, sizeof(rows));
      int readColReturn = read(shared_mem_fd, &cols, sizeof(cols));
      if(readRowReturn == -1 || readColReturn == -1){ 
        perror("ERROR Reading rows/cols\n"); 
        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        exit(1);
      }
      
      //mmap
      gmp=(goldMine*)mmap(nullptr, sizeof(goldMine)+rows*cols, PROT_READ|PROT_WRITE, MAP_SHARED, shared_mem_fd, 0);
      if(gmp == MAP_FAILED){
        cerr << "Error on mmap\n";
        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        exit(1);
      }
      
      //iterate through gmp->players looking for available spot 
      unsigned char currentBit = G_PLR0;
      while(!(currentBit & gmp->players) == 0){
        currentBit <<= 1;
      }
      
      if((int)currentBit == 32){
        cerr << "No available player spot !\n";
        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int syscall_result = sem_post(mysemaphore); // release the semaphore
        if(syscall_result==-1) { 
          perror("Failed sem_post"); 
        }
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
          exit(1);
      }
      gmp->players |= currentBit; 
      current_player = currentBit;  

      //Find Random position for player 2
      player_position = (rand() % (rows*cols));

      while(gmp->map[player_position] != 0){
        player_position = (rand() % (rows*cols));
      }
      
      gmp->map[player_position] = currentBit;

      syscall_result = sem_post(mysemaphore); // release the semaphore
      if(syscall_result==-1) { 
        perror("Failed sem_post"); 
        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        return 2; 
      } 
      
      try {
        pointer_to_rendering_map= make_unique<Map>(gmp->map, rows, cols);
      }catch(const std::exception& e) {
        cerr << e.what() << '\n';
        // if no players remain, close and delete
        // shared memory and semaphore

        int syscall_result=sem_wait(mysemaphore); //"grab" or "take" a semaphore
        if(syscall_result==-1) { 
          perror("Failed sem_wait"); 
          int closeError = close(shared_mem_fd);
          if(closeError == -1){
            perror("ERROR closing shared memory");
          }
          int semCloseError = sem_close(mysemaphore); 
          if(semCloseError == -1){
            perror("ERROR closing semaphore");
          }
          return 2; 
        }

        gmp->map[player_position] = 0;
        gmp->players &= ~current_player;

        syscall_result = sem_post(mysemaphore); // release the semaphore
        if(syscall_result==-1) { 
        perror("Failed sem_post"); 
        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        return 2; 
        }

        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }

        if(gmp->players == 0)
        {
          int shmUnlinkError = shm_unlink("/goldmemory"); 
          if(shmUnlinkError == -1){
            perror("ERROR unlinking shared memory");
          }
          int semUnlinkError = sem_unlink("/goldchase_semaphore"); 
          if(semUnlinkError == -1){
            perror("ERROR unlinking semaphore");
          }
        }
        exit(1);
      } 
         
    }else{
        cerr << "Incorrect Number of arguments\n";
        return 2;
    }

  
  char ch;
  int prev_position=0;
  bool foundGold = false;
  bool wasPureGold = false;
  bool wasFoosGold = false;
  while((ch=pointer_to_rendering_map->getKey()) != 'Q'){
          
          if (ch == 'l'){
            if((((player_position%gmp->cols) + 1) >= gmp->cols) && foundGold){
              pointer_to_rendering_map->postNotice("You Won !");
              break;
            }
            if((((player_position%gmp->cols)) < gmp->cols - 1) && (player_position + 1 < gmp->cols*gmp->rows) && (gmp->map[player_position + 1] != G_WALL)){
              prev_position = player_position;
              player_position += 1;              
            }
          }else if(ch == 'k'){
              if(((player_position - gmp->cols) < 0) && foundGold){
              pointer_to_rendering_map->postNotice("You Won !");
              break;
              }
              if(((player_position - gmp->cols) >= 0) && (gmp->map[player_position - gmp->cols] != G_WALL)){
              prev_position = player_position;
              player_position -= gmp->cols;              
            }
          }else if(ch == 'j'){
              if(((player_position + gmp->cols) >= (gmp->rows*gmp->cols)) && foundGold){
              pointer_to_rendering_map->postNotice("You Won !");
              break;
              }
              if(((player_position + gmp->cols) < (gmp->rows*gmp->cols)) && (gmp->map[player_position + gmp->cols] != G_WALL)){
              prev_position = player_position;
              player_position += gmp->cols;             
            }
          }else if(ch == 'h'){
              if((((player_position%gmp->cols) - 1) < 0) && foundGold){
              pointer_to_rendering_map->postNotice("You Won !");
              break;
              }
              if((((player_position%gmp->cols)) > 0) && (player_position - 1 >= 0) && (gmp->map[player_position - 1] != G_WALL)){
              prev_position = player_position;
              player_position -= 1;           
            }
          }
          
          int syscall_result=sem_wait(mysemaphore); //"grab" or "take" a semaphore
          if(syscall_result==-1) { 
            perror("Failed sem_wait"); 
            int closeError = close(shared_mem_fd);
            if(closeError == -1){
              perror("ERROR closing shared memory");
            }
            int semCloseError = sem_close(mysemaphore); 
            if(semCloseError == -1){
              perror("ERROR closing semaphore");
            }
            exit(1);
          }
          
          
          if(gmp->map[player_position] & G_ANYP){
            gmp->map[player_position] |= current_player;
          }else{
            if(gmp->map[player_position] == G_GOLD){
              wasPureGold = true;
            }
            if(gmp->map[player_position] == G_FOOL){
              wasFoosGold = true;
            }
            gmp->map[player_position] = current_player;
          }
          
          if(gmp->map[prev_position] != current_player){
            gmp->map[prev_position]  &= ~current_player;
          }else{
            gmp->map[prev_position] = 0;
          }
          
          syscall_result=sem_post(mysemaphore); // release the semaphore
          if(syscall_result==-1) { 
            perror("Failed sem_post");
            int closeError = close(shared_mem_fd);
            if(closeError == -1){
              perror("ERROR closing shared memory");
            }
            int semCloseError = sem_close(mysemaphore); 
            if(semCloseError == -1){
              perror("ERROR closing semaphore");
            }
            exit(1);
            }

            pointer_to_rendering_map->drawMap();

            if(wasPureGold){
              foundGold = true;
              pointer_to_rendering_map->postNotice("Yayy! You found it!");
              wasPureGold = false;
            }else if(wasFoosGold){
               pointer_to_rendering_map->postNotice("You found Fool gold :( Try again !");
               wasFoosGold = false;
            }       
          
        }

        //Player quits
        int syscall_result=sem_wait(mysemaphore); //"grab" or "take" a semaphore
          if(syscall_result==-1) { 
            perror("Failed sem_wait");
            int closeError = close(shared_mem_fd);
            if(closeError == -1){
              perror("ERROR closing shared memory");
            }
            int semCloseError = sem_close(mysemaphore); 
            if(semCloseError == -1){
              perror("ERROR closing semaphore");
            }
            exit(1);
          }

        gmp->map[player_position] = 0;
        gmp->players &= ~current_player; //Turn off player bit

        syscall_result=sem_post(mysemaphore); // release the semaphore
        if(syscall_result==-1) { perror("Failed sem_post"); 
        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
        }
        exit(1);
        }

        int semCloseError = sem_close(mysemaphore); 
        if(semCloseError == -1){
          perror("ERROR closing semaphore");
          exit(1);
        }
    
        // if no players remain, close and delete
        // shared memory and semaphore
        if(gmp->players == 0){
        int closeError = close(shared_mem_fd);
        if(closeError == -1){
          perror("ERROR closing shared memory");
        }
        int shmUnlinkError = shm_unlink("/goldmemory"); 
        if(shmUnlinkError == -1){
          perror("ERROR unlinking shared memory");
        }
        int semUnlinkError = sem_unlink("/goldchase_semaphore");
        if(semUnlinkError == -1){
          perror("ERROR unlinking semaphore");
        }
        }

    return 0;
}
