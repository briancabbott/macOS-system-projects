require_relative '../builder/spec_builder'
require_relative '../writer/spec_writer'
require_relative '../spec_file'
require_relative 'project'
require_relative 'configurations'
require_relative 'spec_scripts_processor'
require 'xcodeproj'
require 'awesome_print'
require 'paint'

module StructCore
	class SpecProcessor
		def initialize(project_file, dry_run = false, selected_variants = [])
			return if project_file.to_s.empty?
			@project_file = project_file
			@dry_run = dry_run
			@selected_variants = selected_variants || []
		end

		# @param project_component [StructCore::Processor::ProjectComponent]
		# @param scripts_component [StructCore::SpecScriptsProcessor]
		def process(project_component = nil, scripts_component = nil)
			full_project_path, target_structure = resolve_project_data @project_file
			project_component ||= StructCore::Processor::ProjectComponent.new(target_structure, File.dirname(full_project_path))
			scripts_component ||= StructCore::SpecScriptsProcessor.new

			source_dsl = process_source_dsl full_project_path
			scripts_component.pre_generate source_dsl

			outputs = project_component.process source_dsl, @selected_variants
			return if outputs.empty?

			puts "\n" unless @dry_run

			process_outputs outputs, scripts_component, source_dsl
			outputs
		end

		def resolve_project_data(project_file)
			full_project_path = project_file.to_s
			full_project_path = File.join(Dir.pwd, full_project_path) unless Pathname.new(full_project_path).absolute?

			if full_project_path.end_with? '.xcodeproj'
				target_structure = :spec
			elsif full_project_path.end_with? '.yaml'
				target_structure = :xcodeproj
			elsif full_project_path.end_with? '.yml'
				target_structure = :xcodeproj
			elsif full_project_path.end_with? '.json'
				target_structure = :xcodeproj
			elsif File.basename(full_project_path) == 'Specfile'
				target_structure = :xcodeproj
			else
				raise StandardError.new "Unrecognised project format: #{File.basename(full_project_path)}"
			end

			[full_project_path, target_structure]
		end

		def process_source_dsl(full_project_path)
			# TODO: Replace with Spec & DSL processor
			source_dsl = nil
			source_dsl = StructCore::Specfile.parse full_project_path if full_project_path.end_with?('.yml')
			source_dsl = StructCore::Specfile.parse full_project_path if full_project_path.end_with?('.yaml')
			source_dsl = StructCore::Specfile.parse full_project_path if full_project_path.end_with?('.json')
			source_dsl = StructCore::SpecBuilder.build full_project_path if full_project_path.end_with?('Specfile')
			source_dsl = Xcodeproj::Project.open full_project_path if full_project_path.end_with?('.xcodeproj')

			raise StandardError.new "Unrecognised project format: #{File.basename(full_project_path)}" if source_dsl.nil?

			source_dsl
		end

		def process_outputs(outputs, scripts_component, source_dsl)
			outputs.each { |output|
				if @dry_run
					print output.dsl if output.dsl.is_a?(Xcodeproj::Project)
					print output.dsl if output.dsl.is_a?(Xcodeproj::XCScheme)
					print output.dsl, raw: true if output.dsl.is_a?(StructCore::Specfile)
				else
					StructCore::Specwriter.new.write_spec output.dsl, output.path unless output.path.end_with?('.xcodeproj', '.xcscheme')
					output.dsl.save output.path if output.path.end_with? '.xcodeproj'
					output.dsl.save_as output.options[:project], output.options[:name] if output.path.end_with? '.xcscheme'
					puts Paint["Saved '#{output.path}'"]
				end

				scripts_component.post_generate source_dsl, output.dsl
			}
		end

		def print(dsl, options = {})
			ap dsl, options
		end

		private :resolve_project_data
		private :process_source_dsl
		private :print
	end
end