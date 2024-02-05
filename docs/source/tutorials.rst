Tutorials
---------

This tutorial emphasises the abilities and usage of Xeus-cpp. Let's get
started! The tutorial demonstrates examples of C++ and C++-python integration
on Jupyter kernels.

1. Install the xeus-cpp from source using conda-forge for fetching dependencies.

2. Launch the Jupyter Notebook with C++-14, C++-17 kernels available.

3. In a code cell, write the C++ code.

4. Run the code cell. The xeus-cpp kernel will compile and execute the code.
   The output would be displayed below the respective cell.

C++
===

.. image:: sum.png
   :width: 200px
   :height: 100px
   :scale: 250 %
   :align: center

In this example, you can modify the code and execute the cell again to see the
updated results. For example, you might change the values of `a` and `b`
(function arguments) of the function `sum` to see expected output. You can try
out different examples to explore the features of C++.

The output of the cell will be displayed below once the code is executed and
xeus-cpp provides an interactive environment, where you can promptly make
the changes and prototype in C++.

C++-Python Integration:
=======================

.. image:: integration-demo.png
   :width: 200px
   :height: 100px
   :scale: 400 %
   :align: center

In this example, we are emphasising the concept of C++-Python integration, where
we use Python and C++ in the same session, sharing variables, scopes, and features.
Here, we have used variables (`new_var1`, `new_var2`, `new_var3`)
in python which have been initialised in C++. In the following context, we have
tried the vice versa as well of using the variables in Python (`new_python_var`)
which have been defined in C++.

.. code-block:: bash

   In[1] #include <iostream>

   In[2] void Cpp() {
         std::cout << "Hello World C++!" << std::endl;
         }

   In[3] %%python

   In[4] Cpp()

We are taking another example of working with functions that can be used in
to call functions defined in C++ in python code with help of magic commands.

The project is under development and is subject to changes as C++-Python integration
within the same Jupyter cell is quite challenging. Please ensure that you have
xeus-cpp and your Jupyter environment set up before attempting to run C++
code. The installation instructions can be found in this documentation for xeus-cpp.
