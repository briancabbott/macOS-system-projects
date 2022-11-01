#
#  BuildPListFiles.rb
#  CertificateTool
#
#  Copyright 2012-2015 Apple Inc. All rights reserved.
#

require 'fileutils'

@verbose = false

def do_output_str(str, header = false)
    return if !@verbose
    
    puts "=====================================================" if header
    puts str if !str.nil?
end

do_output_str(nil, true)
do_output_str(" ")
do_output_str "Entering BuildPlistFiles.rb"
do_output_str(nil, true)
do_output_str(" ")

build_dir = ENV["BUILT_PRODUCTS_DIR"]
sdk_name = ENV["SDK_NAME"]
top_level_directory = ENV["PROJECT_DIR"]
project_temp_dir = ENV["PROJECT_TEMP_DIR"]

do_output_str(nil, true)
do_output_str("Environment variables")
do_output_str(" ")

do_output_str "build_dir = #{build_dir}"
do_output_str "sdk_name = #{sdk_name}"
do_output_str "top_level_directory = #{top_level_directory}"
do_output_str(nil, true)
do_output_str(" ")

top_level_directory = File.join(top_level_directory, "..")
output_directory = File.join(build_dir, "BuiltAssets")
tool_path = File.join(project_temp_dir, "CertificateTool")

do_output_str(nil, true)
do_output_str("Path variables")
do_output_str "top_level_directory = #{top_level_directory}"
do_output_str "output_directory = #{output_directory}"
do_output_str "tool_path = #{tool_path}"
do_output_str(nil, true)
do_output_str(" ")

cmd_str = tool_path + " --top_level_directory " + "'" + top_level_directory + "' " + " --output_directory " + "'" + output_directory + "'"
do_output_str(nil, true)
do_output_str "Executing command: #{cmd_str}"
do_output_str(nil, true)
do_output_str(" ")

`#{cmd_str}`
result = $?
exit_status = result.to_i
if exit_status != 0
    puts result
    raise "Failed command: #{cmd_str}"
end

#products_path = File.join(build_dir, "..")
#FileUtils.cp_r output_directory, products_path

do_output_str(nil, true)
do_output_str "Completed BuildPlistFiles.rb"
do_output_str(nil, true)
do_output_str(" ")


    