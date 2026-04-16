#include <stdio.h>
#include<stdlib.h>
double **read(int n){
   int **matrix=(double **)malloc(sizeof(double *)*n);
   int i,j;
   for(i=0;i<n;i++){
       matrix[i]=(double *)malloc(sizeof(double )*n);
       for(j=0;j<n;j++){
           scanf("%d",&matrix[i][j]);
       }
   }
   return matrix;
}

double *vect_medii(double **matrix,int n){
   int i,j;
   float s;
   int *v_medii=(double *)malloc(sizeof(double )*n);
   for(j=0;j<n;j++){
       s=0;
       for(i=0;i<n;i++){
           s=s+matrix[i][j];
       }
       v_medii[j]=s/n;
   }
   return v_medii;
}

int prelucrare(double **matrix, int n, int *indecsi, int m){
   int *v_medii=vect_medii(matrix,n);
   int i;
   int k=0;
   float max=v_medii[0];
   for(i=1;i<m;i++){
       if(v_medii[i]>max){
           max=v_medii[i];
       }
   }
   for(i=0;i<n;i++){
       if(v_medii[i]==max){
           indecsi[k]=v_medii[i];
           k++;
       }
   }
   printf("%f\n",max);
   //noua ni se cere sa returnam vectorul de indecsi care este de tip int
   return indecsi;
}
int main(){
   int n;
   scanf("%d",&n);
   double **matrix=read(n);
   int *indecsi=(int *)malloc(sizeof(int)*n);
   int v=prelucrare(matrix,n,indecsi,n);
}


