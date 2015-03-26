#!/bin/bash
# This code tests pentatope actually runs and produces some results
# for all example scenes.

readonly EXEC_PENTATOPE="sudo docker run -t -i --rm -v /home/xyx/repos/pentatope:/root/local xanxys/pentatope-dev /root/local/build/pentatope"

print_pass() {
    echo -e "\033[0;32m[PASS]\033[0m"
}

print_fail() {
    echo -e "\033[0;31m[FAIL]\033[0m"
}

# Reduce sampling settings and modify output path for clean testing.
# Output will be "$temp_dir/cornell.png"
create_cornell_task_for_test() {
    local temp_dir=$1
    cat example/cornell_tesseract.prototxt | \
        sed -e 's|output_path:.*|output_path: "/root/local/'$temp_dir'/cornell.png"|' | \
        sed -e 's/sample_per_pixel:.*/sample_per_pixel:3/' > \
        $temp_dir/cornell.prototxt
}

test_examples() {
    local temp_dir=$(mktemp -p ./ -d)
    echo "Using temporary directory $temp_dir"

    echo -e "[TEST] anim_demo"
    example/animation_demo.py --test --output $temp_dir/anim_demo.pb \
        --render_output /root/local/$temp_dir/anim_demo.png
    $EXEC_PENTATOPE --render /root/local/$temp_dir/anim_demo.pb
    if $(test -e $temp_dir/anim_demo.png); then
        print_pass
    else
        print_fail
    fi

    echo "[TEST] cornell_tesseract"
    create_cornell_task_for_test $temp_dir
    $EXEC_PENTATOPE --render /root/local/$temp_dir/cornell.prototxt
    if $(test -e $temp_dir/cornell.png); then
        print_pass
    else
        print_fail
    fi

    rm -rfv $temp_dir
}

test_examples
