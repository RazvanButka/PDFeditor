#include <iostream>
using namespace std;

static void fun(){
   static int x = 2;
   cout << " x = " << ++x << '\n';
}

int main(int argc, char const * argv[]){
   int N_CLS = 5;
   for(;N_CLS--;){
     fun();
   }
}

