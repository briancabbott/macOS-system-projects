require_relative 'processor_component'
require_relative 'target_sources_cache'

module StructCore
	module Processor
		class TargetSourceComponent
			include ProcessorComponent

			def initialize(structure, working_directory)
				super(structure, working_directory)
			end

			def process(source, target_dsl = nil, group_dsl = nil, sources_cache = nil)
				output = nil

				output = process_xc_source source if structure == :spec
				output = process_spec_source source, target_dsl, group_dsl, sources_cache || TargetSourcesCache.new if structure == :xcodeproj && !target_dsl.nil? && !group_dsl.nil?

				output
			end

			# @param source [Xcodeproj::Project::Object::PBXFileReference]
			def process_xc_source(source)
				path = source.real_path.to_s.sub(@working_directory, '')
				path = path.slice(1, path.length) if path.start_with? '/'

				path
			end

			# @param source [String]
			# @param target_dsl [Xcodeproj::Project::Object::PBXNativeTarget]
			# @param group_dsl [Xcodeproj::Project::Object::PBXGroup]
			def process_spec_source(source, target_dsl, group_dsl, sources_cache)
				file = source.sub(@working_directory, '')
				file = file.slice(1, file.length) if file.start_with? '/'

				return add_source_reference(file, target_dsl) if file.end_with?('.framework', '.a')
				native_file = sources_cache.ref source, file, group_dsl

				build_file = nil
				if file.end_with? '.swift', '.m', '.mm', '.c', '.cpp', '.cxx'
					target_dsl.source_build_phase.files_references << native_file
					build_file = target_dsl.add_file_references([native_file]).first
				elsif target_dsl.product_reference.path.end_with?('.framework') && file.end_with?('.h')
					header = target_dsl.headers_build_phase.add_file_reference native_file, true
					header.settings = { 'ATTRIBUTES' => %w(Public) }
				elsif file.end_with? '.entitlements'
					return
				else
					target_dsl.add_resources [native_file]
				end

				build_file || native_file
			end

			def add_source_reference(file, target_dsl)
				if file.end_with? '.framework'
					framework_group = target_dsl.project.frameworks_group.groups.find { |group| group.display_name == '$local' }
					framework_group = target_dsl.project.frameworks_group.new_group '$local', nil, '<group>' if framework_group.nil?

					# The 'Embed Frameworks' phase is missing by default from the Xcodeproj template, so we have to add it.
					embed_phase = target_dsl.build_phases.find { |b| b.is_a?(Xcodeproj::Project::Object::PBXCopyFilesBuildPhase) && b.name == 'Embed Frameworks' }
					if embed_phase.nil?
						embed_phase = target_dsl.project.new(Xcodeproj::Project::Object::PBXCopyFilesBuildPhase)
						embed_phase.name = 'Embed Frameworks'
						embed_phase.symbol_dst_subfolder_spec = :frameworks
						target_dsl.build_phases.insert(target_dsl.build_phases.count, embed_phase)
					end

					framework = framework_group.new_file file
					(embed_phase.add_file_reference framework).settings = { 'ATTRIBUTES' => %w(CodeSignOnCopy RemoveHeadersOnCopy) }
					target_dsl.frameworks_build_phase.add_file_reference framework
				elsif file.end_with? '.a'
					target_dsl.frameworks_build_phase.add_file_reference file
				end

				nil
			end
		end
	end
end