#include <iostream>

using namespace std;

void f(int n)
{ int i;
for(i=n;i>=1;i--)
{ f(n-1);

cout<<i;// | printf(“%d”,i);
}
}

int main(){
	f(3);
	cout << endl;
}
