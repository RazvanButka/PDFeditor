// Bubble sort in C

//#include <stdio.h>
//#include <time.h>

// perform the bubble sort
void bubbleSort(int array[], int size) {

  // loop to access each array element
  for (int step = 0; step < size - 1; ++step) {

    // loop to compare array elements
    for (int i = 0; i < size - step - 1; ++i) {

      // compare two adjacent elements
      // change > to < to sort in descending order
      if (array[i] > array[i + 1]) {

        // swapping occurs if elements
        // are not in the intended order
        int temp = array[i];
        array[i] = array[i + 1];
        array[i + 1] = temp;
      }
    }
  }
}

// print array
void printArray(int array[], int size) {
  for (int i = 0; i < size; ++i) {
    printf("%d  ", array[i]);
  }
  printf("\n");
}

int main() {
  int data[] = {828744, 90531, 332815, -610102, 509608, 346735, -824121, 800654, 271429, -518540, -364519, -171752, 461729, -346068, 538831, 327461, 344631, -23527, 69043, -616159, -966613, -703729, -507014, -947850, -406491, -227059, 341775, -703736, -580502, 985452, 904735, 130467, 438514, 282099, 18927, 110405, -598440, 665444, 570013, 710798, -437489, 859630, 367395, 873422, 438166, 831637, -438703, 60596, -47733, 287312, 381205, -477547, 62591, 415925, 377221, 254243, 682259, 770626, 455112, 958008, 413328, -289090, 997440, -106670, -690902, 870213, -137458, -149036, 3055, -670860, 51445, 610321, -809744, 16028, -236887, 588466, 681319, -907165, -147419, -935231, -62464, -154118, -81805, -412195, 111259, -387129, -653798, -705855, -732300, 532494, 82807, 667062, 265136, 88445, 890467, -718008, 205376, 56217, -655567, 674347};
  clock_t start, end;
  double cpu_time_used;

  // find the array's length
  int size = sizeof(data) / sizeof(data[0]);

  start = clock();
  bubbleSort(data, size);
  end = clock();
  cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

  //printf("Sorted Array in Ascending Order:\n");
  //printArray(data, size);

  printf("%lf\n", cpu_time_used);

}
