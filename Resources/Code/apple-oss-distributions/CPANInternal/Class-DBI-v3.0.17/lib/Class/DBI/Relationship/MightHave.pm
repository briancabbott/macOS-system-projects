package Class::DBI::Relationship::MightHave;

use strict;
use warnings;

use base 'Class::DBI::Relationship';

sub remap_arguments {
	my ($proto, $class, $method, $f_class, @methods) = @_;
	$class->_require_class($f_class);
	return ($class, $method, $f_class, { import => \@methods });
}

sub triggers {
	my $self = shift;

	my $method = $self->accessor;

	return (
		before_update => sub {
			if (my $for_obj = shift->$method()) { $for_obj->update }
		},

		before_delete => sub {
			if (my $for_obj = shift->$method()) { $for_obj->delete }
		},
	);
}

sub methods {
	my $self = shift;
	my ($class, $method) = ($self->class, $self->accessor);
	return (
		$method => $self->_object_accessor,
		map { $_ => $self->_imported_accessor($_) } @{ $self->args->{import} }
	);
}

sub _object_accessor {
	my $rel = shift;
	my ($class, $method) = ($rel->class, $rel->accessor);
	return sub {
		my $self = shift;
		my $meta = $class->meta_info($rel->name => $method);
		my ($f_class, @extra) =
			($meta->foreign_class, @{ $meta->args->{import} });
		return
			if defined($self->{"_${method}_object"})
			&& $self->{"_${method}_object"}
			->isa('Class::DBI::Object::Has::Been::Deleted');
		$self->{"_${method}_object"} ||= $f_class->retrieve($self->id);
	};
}

sub _imported_accessor {
	my ($rel, $name) = @_;
	my ($class, $method) = ($rel->class, $rel->accessor);
	return sub {
		my $self = shift;
		my $meta = $class->meta_info($rel->name => $method);
		my ($f_class, @extra) =
			($meta->foreign_class, @{ $meta->args->{import} });
		my $for_obj = $self->$method() || do {
			return unless @_;    # just fetching
			my $val = shift;
			$f_class->insert(
				{ $f_class->primary_column => $self->id, $name => $val });
			$self->$method();
		};
		$for_obj->$name(@_);
	};
}

1;
