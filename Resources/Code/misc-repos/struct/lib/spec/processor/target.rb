require_relative 'processor_component'
require_relative 'target_configurations'
require_relative 'target_sources'
require_relative 'target_resources'
require_relative 'target_references'
require_relative 'target_scripts'

module StructCore
	module Processor
		class TargetComponent
			include ProcessorComponent

			def initialize(structure, working_directory)
				super(structure, working_directory)
				@configurations_component = TargetConfigurationsComponent.new(@structure, @working_directory)
				@sources_component = TargetSourcesComponent.new(@structure, @working_directory)
				@resources_component = TargetResourcesComponent.new(@structure, @working_directory)
				@references_component = TargetReferencesComponent.new(@structure, @working_directory)
				@scripts_component = TargetScriptsComponent.new(@structure, @working_directory)
			end

			def process(target, target_dsl = nil, dsl = nil, sources_cache = nil)
				output = nil
				output = process_xc_target target, target_dsl if structure == :spec && !target_dsl.nil?
				output = process_spec_target target, target_dsl, dsl, sources_cache if structure == :xcodeproj && !target_dsl.nil? && !dsl.nil?

				output
			end

			# @param target [Xcodeproj::Project::PBXNativeTarget]
			# @param target_dsl [StructCore::Specfile::Target]
			def process_xc_target(target, target_dsl)
				target_dsl.configurations = @configurations_component.process target, target_dsl
				target_dsl.source_dir = @sources_component.process target, target_dsl
				target_dsl.res_dir = @resources_component.process target
				target_dsl.references = @references_component.process target
				target_dsl
			end

			# @param target [StructCore::Specfile::Target]
			# @param target_dsl [Xcodeproj::Project::PBXNativeTarget]
			# @param dsl [Xcodeproj::Project]
			def process_spec_target(target, target_dsl, dsl, sources_cache = nil)
				@configurations_component.process target, target_dsl, dsl
				@sources_component.process target, target_dsl, dsl, sources_cache
				@resources_component.process target, target_dsl, dsl
				@references_component.process target, target_dsl, dsl
				@scripts_component.process target, target_dsl, dsl
			end
		end
	end
end