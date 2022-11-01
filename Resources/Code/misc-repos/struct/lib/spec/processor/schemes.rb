require_relative 'processor_component'
require_relative 'scheme_build_action'
require_relative 'scheme_test_action'
require_relative 'scheme_archive_action'
require_relative 'scheme_profile_action'
require_relative 'scheme_launch_action'
require_relative 'scheme_analyze_action'

module StructCore
	module Processor
		class SchemesComponent
			include ProcessorComponent

			def initialize(structure, working_directory, build_component = nil, test_component = nil, archive_component = nil, profile_component = nil, launch_component = nil, analyze_component = nil)
				super(structure, working_directory)
				@build_action_component = build_component
				@build_action_component ||= SchemeBuildActionComponent.new @structure, @working_directory
				@test_action_component = test_component
				@test_action_component ||= SchemeTestActionComponent.new @structure, @working_directory
				@archive_action_component = archive_component
				@archive_action_component ||= SchemeArchiveActionComponent.new @structure, @working_directory
				@profile_action_component = profile_component
				@profile_action_component ||= SchemeProfileActionComponent.new @structure, @working_directory
				@launch_action_component = launch_component
				@launch_action_component ||= SchemeLaunchActionComponent.new @structure, @working_directory
				@analyze_action_component = analyze_component
				@analyze_action_component ||= SchemeAnalyzeActionComponent.new @structure, @working_directory
			end

			def process(project, dsl = nil)
				output = []

				output = process_xc_schemes project if structure == :spec
				output = process_spec_schemes project, dsl if structure == :xcodeproj && !dsl.nil?

				output
			end

			def process_xc_schemes(project) end

			# @param project [StructCore::Specfile]
			# @param dsl [Xcodeproj::Project]
			def process_spec_schemes(project, dsl)
				(project.schemes || []).map { |scheme|
					scheme_dsl = Xcodeproj::XCScheme.new

					scheme_dsl.build_action = @build_action_component.process scheme.build_action, scheme_dsl.build_action, dsl.targets unless scheme.build_action.nil?
					scheme_dsl.test_action = @test_action_component.process dsl, scheme.test_action, scheme_dsl.test_action, dsl.targets unless scheme.test_action.nil?
					scheme_dsl.archive_action = @archive_action_component.process scheme.archive_action, scheme_dsl.archive_action unless scheme.archive_action.nil?
					scheme_dsl.profile_action = @profile_action_component.process scheme.profile_action, scheme_dsl.profile_action, dsl.targets unless scheme.profile_action.nil?
					scheme_dsl.launch_action = @launch_action_component.process scheme.launch_action, scheme_dsl.launch_action, dsl.targets unless scheme.launch_action.nil?
					scheme_dsl.analyze_action = @analyze_action_component.process scheme.analyze_action, scheme_dsl.analyze_action unless scheme.analyze_action.nil?

					process_scheme_configurations scheme_dsl, project
					[scheme_dsl, scheme.name, dsl.path.to_s]
				}
			end

			def process_scheme_configurations(scheme_dsl, project)
				# Resolve default build_configuration values to the spec's build configuration names
				project_configurations = project.configurations.map(&:name)
				default_build_configuration = 'Release'
				default_build_configuration = project_configurations.first unless project_configurations.empty?
				[scheme_dsl.test_action, scheme_dsl.archive_action, scheme_dsl.profile_action, scheme_dsl.launch_action, scheme_dsl.analyze_action].select { |action|
					!(project_configurations.include? action.build_configuration)
				}.each { |action|
					action.build_configuration = XC_SCHEME_CONFIGURATION_MAP[action.build_configuration]
					action.build_configuration = default_build_configuration if action.build_configuration.nil?
				}
			end
		end
	end
end