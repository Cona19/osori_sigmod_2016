#!/bin/bash
#usage : ./make_result_file.sh test_case_number 

python generator.py

cat init-file.txt > total-file.txt
echo "S" >> total-file.txt
cat workload-file.txt >> total-file.txt
echo "F" >> total-file.txt

./spp < total-file.txt > result-file.txt

mv init-file.txt init-file$1.txt
mv workload-file.txt workload-file$1.txt

#tail command is used to remove 'R' message
tail -n +2 result-file.txt > result-file$1.txt
rm -rf result-file.txt
rm -rf total-file.txt

echo "Completed!"
