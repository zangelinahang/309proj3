# Project 3 Design Log

### Author: Angelina Zhang

**P2 code used:** instructor reference using `-DUSEP2REFERENCE=ON`  
**Tools used:** ChatGPT was used to assist in reviewing the project requirements, thinking about implementations, assisting with testing, and assisting with reviewing the code and design log. The project was then compiled and tested using the supplied CMake and testing commands.

### 1. Hash Table

In this project, I implemented `ToolRegistry` based on a hash table with separate chaining. The hash table uses a `std::vector` of buckets, each of which is another `std::vector` of `std::unique_ptr`.

Separate chaining was used because it is simple. If there is a collision, then two tool names will hash to the same bucket and both tools will be stored in that bucket. To locate a tool, the tool name is hashed, the corresponding bucket is located, then the data structures in that bucket are searched.

The following hash function was used: 64-bit FNV-1a. The function can iterate over every character in the tool name. It calculates the XOR of the characters with the current hash value and then multiplies the result with the FNV prime, making the result dependent on the characters and their order.

The table starts with 16 buckets. Prior to inserting a new tool, the program tests if the new insertion would make the load factor bigger than 0.75. If so, the number of buckets are doubled, and all the current tools are rehashed into the new table.

### The tests on 10,000 tools named `tool_0` through `tool_9999` returned:

- bucket count: **16,384**
- load factor: **0.610352**
- maximum chain length: **6**
- weak hash maximum chain length: **9000**

The weak hash for the comparison was `s[0] + s.size()`. Many of the tool names share the same first character and look to have the same length. The weak hash results in a lot of tools going to the same buckets. FNV-1a makes them all go to different buckets.

The amortized complexity of `add()` is O(1). Most of the time, adding an element involves hashing its name and inserting it into one bucket. Resizing requires O(n) cost, but it is not performed for every insertion. So, the cost of resizing is amortized over many normal insertions.

### 2. Pointer Stability

Every tool is stored in the registry as `std::unique_ptr`. The pointer returned from `find()` points to the actual `Tool` object on the heap.

When resizing, the `unique_ptr` objects are moved into a new bucket. The `Tool` objects, however, do not get moved. It's just the ownership of the pointers that changes from one bucket to the next.

Consequently, the `Tool*` returned before a resize will point to the correct tool even after a resize. This was verified by saving the pointer of the first tool, adding 10,000 tools, and verifying that the tool's pointer had not changed.

### 3. Calculator Trace

The calculator employs two stacks. One stack maintains numbers whereas the other one stacks operators.

### For the expression:

`2 * (3 + 4) - 5`

Push the first value, `2`, onto the value stack, push the `*` onto the operator stack, and push the `(` onto the operator stack.

Then, `3` is pushed onto the value stack. As the top of the operator stack is `(`, the operator `+` is pushed. Next, `4` is pushed onto the value stack.

On reaching `)`, the calculator will use the operators until reaching a `(`. In this example it has to calculate `3 + 4` first. This produces `7` and the `(` is removed.

Now:

- value stack: `2 7`
- operator stack: `*`

Once you reach the `-`, it will pop out the operator as it has higher precedence than the `-`. This evaluates `2 * 7`, which equals `14`. Then it pushes the `-` operator.

Finally, `5` is pushed onto the value stack. At the end of the expression, the remaining subtraction is carried out:

### `14 - 5 = 9`

The final result is **9**.

When an operator is encountered, the right operand is extracted first, left operand second. This is of course significant in the case of subtraction and division. Of course, since the greater-than-or-equal precedence is used, the statement `10 - 4 - 3` is evaluated left to right.

### 4. Errors as Values

For this project, errors of the tool are not returned as errors but are treated as a normal output.

One case is that a required file doesn't exist, or the path leads outside the workspace, or a calculator expression involves division by zero. These are examples of errors that can arise during everyday use of the tool.

The functions return `false` or call `ToolResult::failure()` when this occurs. This enables the harness to pass the error back to the model. The model is then able to read the error message and take action.

Exceptions should be used more for errors due to the wrong C++ code. For example, asking for the wrong accessor on a JSON value might be due to a programming error and not a normal tool failure.

This separation enables regular tool errors to not abort the program.

### 5. What I Would Design Differently

One change I might make is to have the `Tool` name be stored with each `unique_ptr`.

In the current implementation, search and rehash functions call `Tool::name()` only if they need the key. Our current implementation currently works fine, but storing the key would prevent us from creating the tool name again while a hash table is searched over.

This would use a little more memory, but could also make certain registry operations more efficient.