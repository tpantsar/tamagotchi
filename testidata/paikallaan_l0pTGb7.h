float data[7][100] = {
    {0, 0.01, 0.02, -0.98, 0.95, 0.53, 0.51},
    {1, -0.00, 0.02, -0.98, 1.01, 0.84, -0.51},
    {2, -0.01, 0.02, -0.99, 1.03, 0.03, -0.35},
    {3, -0.00, 0.00, -1.00, 0.37, 0.03, 0.33},
    {4, -0.00, 0.02, -1.00, 0.53, 0.01, 1.17},
    {5, 0.00, 0.02, -0.99, 0.63, 0.42, 0.67},
    {6, 0.01, 0.01, -0.97, 0.91, 1.23, -0.02},
    {7, -0.00, 0.02, -0.97, 1.25, 0.76, 0.20},
    {8, -0.01, 0.02, -0.99, 1.43, 0.24, 0.84},
    {9, -0.01, 0.03, -0.98, 0.92, 0.14, 0.20},
    {10, 0.01, 0.03, -1.00, 0.37, 0.72, 0.48},
    {11, -0.00, 0.02, -0.99, 0.93, 0.82, 1.21},
    {12, 0.02, 0.03, -0.96, 1.76, -0.10, 1.10},
    {13, -0.00, 0.04, -0.98, 1.87, -0.60, -0.22},
    {14, -0.00, 0.03, -0.99, 0.64, 0.96, 0.82},
    {15, -0.01, 0.04, -1.00, 1.16, 0.90, 1.81},
    {16, 0.00, 0.02, -0.97, 1.14, 0.16, 0.15},
    {17, 0.01, 0.03, -1.01, 0.83, 0.24, 1.16},
    {18, 0.02, 0.04, -0.98, 0.82, 0.53, 0.76},
    {19, -0.02, 0.02, -0.97, 1.27, 0.01, 0.08},
    {20, -0.01, 0.02, -0.97, 0.27, 0.24, -0.60},
    {21, -0.00, 0.04, -0.97, 1.26, 0.25, 0.08},
    {22, 0.00, 0.02, -0.97, 0.92, 0.47, 0.67},
    {23, -0.00, 0.03, -0.98, 1.79, -0.12, 1.05},
    {24, -0.00, 0.02, -0.98, 1.55, 0.34, 0.17},
    {25, -0.01, 0.04, -0.98, 0.73, 0.42, -0.21},
    {26, -0.00, 0.03, -1.00, 0.62, 0.45, -0.18},
    {27, 0.00, 0.03, -0.98, 1.48, 0.60, 1.19},
    {28, 0.00, 0.02, -0.98, 1.69, -0.26, -0.47},
    {29, 0.00, 0.00, -1.00, 0.93, 1.46, 0.73},
    {30, 0.01, 0.01, -0.99, 1.52, -0.06, 1.30},
    {31, -0.00, 0.01, -0.97, 0.91, 0.54, 0.40},
    {32, 0.01, 0.03, -0.97, 0.63, 0.51, 0.25},
    {33, 0.01, 0.03, -0.98, 0.72, 0.97, -0.27},
    {34, 0.01, 0.02, -1.00, 0.60, 0.81, 0.93},
    {35, 0.01, 0.02, -0.99, 1.08, -0.03, -0.50},
    {36, 0.01, 0.03, -0.98, 1.08, 1.07, 1.02},
    {37, -0.01, 0.03, -0.99, 0.27, -0.25, 0.08},
    {38, 0.01, 0.02, -0.99, 1.09, 0.50, 0.16},
    {39, -0.00, 0.02, -0.99, 1.14, -0.63, -0.01},
    {40, 0.01, 0.02, -0.99, 0.53, 1.24, 1.43},
    {41, -0.01, 0.02, -0.98, 1.10, 0.69, 0.40},
    {42, 0.02, 0.02, -0.99, 0.51, 0.27, 0.74},
    {43, -0.00, 0.03, -1.00, 1.10, 0.31, 0.18},
    {44, 0.00, 0.04, -0.99, 0.76, 0.74, -0.18},
    {45, -0.01, 0.03, -0.99, 0.61, 0.90, 0.53},
    {46, 0.01, 0.02, -0.99, 1.19, 0.61, -0.62},
    {47, 0.00, 0.03, -0.98, 1.33, -0.02, 0.47},
    {48, 0.00, 0.02, -0.98, 0.97, -0.48, 0.58},
    {49, 0.00, 0.03, -0.97, 0.40, 0.59, 0.60},
    {50, 0.01, 0.02, -0.99, 0.91, 0.66, 0.74},
    {51, 0.01, 0.02, -1.00, 0.56, 0.53, 1.12},
    {52, -0.00, 0.02, -0.97, 0.59, 0.85, 0.37},
    {53, 0.02, 0.02, -1.00, 1.05, 0.89, 0.42},
    {54, -0.01, 0.03, -1.00, 1.37, -0.09, 1.08},
    {55, -0.00, 0.03, -0.99, 1.00, 0.21, 0.28},
    {56, 0.02, 0.03, -0.97, 0.14, -0.14, 0.73},
    {57, -0.01, 0.02, -0.99, -0.10, 0.66, 1.17},
    {58, -0.01, 0.03, -0.99, 1.25, 0.82, 1.44},
    {59, 0.01, 0.04, -0.97, 0.92, 1.17, -0.25},
    {60, 0.01, 0.02, -1.01, 0.72, 0.56, 1.15},
    {61, 0.00, 0.03, -0.98, 0.25, 0.53, -0.08},
    {62, -0.00, 0.01, -1.00, 1.32, 0.24, 0.95},
    {63, 0.01, 0.03, -0.98, 1.16, -0.50, 1.08},
    {64, 0.01, 0.02, -0.98, 0.05, -0.15, 0.23},
    {65, 0.02, 0.02, -0.99, 0.76, 0.85, 0.53},
    {66, -0.01, 0.03, -0.98, 1.18, 0.54, 0.54},
    {67, 0.01, 0.02, -0.99, 0.94, 0.48, 0.57},
    {68, -0.01, 0.05, -1.00, 0.81, 0.55, 0.86},
    {69, 0.00, 0.04, -0.99, 0.58, -0.20, -0.21},
    {70, -0.01, 0.04, -0.99, 0.05, 0.70, 1.56},
    {71, -0.00, 0.03, -0.99, 1.56, 0.92, 0.71},
    {72, 0.00, 0.01, -0.97, 0.73, -0.17, 0.92},
    {73, -0.00, 0.03, -1.00, 0.43, 1.07, 0.90},
    {74, -0.01, 0.04, -0.98, 0.66, 0.33, 1.35},
    {75, -0.00, 0.02, -0.99, 0.60, 1.08, 0.50},
    {76, -0.01, 0.02, -1.00, 0.71, 1.81, 1.53},
    {77, 0.01, 0.03, -0.99, 0.86, -0.05, 0.61},
    {78, -0.01, 0.03, -0.96, 0.83, 0.63, 1.24},
    {79, -0.00, 0.01, -0.97, 0.31, 0.43, 0.29},
    {80, 0.00, 0.02, -0.97, 1.33, 1.02, 1.43},
    {81, -0.00, 0.01, -0.97, 0.37, 0.59, 0.64},
    {82, -0.01, 0.04, -0.98, 0.55, 0.26, 0.32},
    {83, 0.01, 0.02, -0.98, 0.32, 0.18, 0.27},
    {84, 0.02, 0.03, -1.00, 1.07, 0.61, 0.56},
    {85, -0.01, 0.01, -0.98, 1.21, 0.65, 0.64},
    {86, 0.00, 0.02, -0.99, 1.35, 0.76, 0.84},
    {87, -0.01, 0.01, -0.98, 1.08, 0.24, 0.08},
    {88, -0.00, 0.04, -0.98, 0.53, 0.17, -0.13},
    {89, 0.02, 0.01, -0.98, 0.34, -0.14, 0.86},
    {90, 0.01, 0.03, -1.00, 0.47, 0.06, 0.25},
    {91, 0.01, 0.02, -0.99, 1.38, 0.38, 0.76},
    {92, 0.01, 0.03, -0.98, 0.43, -0.14, 0.89},
    {93, 0.01, 0.03, -0.98, 0.02, -0.68, -0.22},
    {94, -0.00, 0.04, -1.01, 0.45, 0.37, 0.53},
    {95, 0.00, 0.03, -0.98, 0.92, 1.91, 0.25},
    {96, 0.00, 0.03, -0.98, 0.60, 0.79, 0.24},
    {97, -0.00, 0.02, -0.96, 1.30, 0.45, 0.82},
    {98, -0.00, 0.03, -0.98, 0.82, -0.01, 0.66},
    {99, 0.03, 0.03, -1.00, 1.39, 0.08, 0.40}};