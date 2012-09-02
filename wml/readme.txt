Whitespace Markup Language:

	Aims::
		* very simple
		* no clutter while writing
		* only indentation counts
		* empty lines have no meaning
		* embedding text is easy
		* everything is a map


	Example:

		title 		"test"
		version 	1

		content::
			unformated text

			newlines count here

		properties:
			time-changed	10:47am
			flags	archive system hidden

		streams:
			stream:
				data::
					some data

					this is nested too
				flags:
					read
					write
					execute

			stream:
				data::
					key names 
					dont have to 
					be unique (see stream)
				flags:
					read
					write:
						users	andreas root



	Grammar::
		root := map

		IDENT, DEIDENT are virtual tokens that control the identation level		
		NEWLINE is a line break

		value := number | identifier | string

		key := value

		map := (key value+ NEWLINE | key ':' ( ':' NEWLINE IDENT textblock DEIDENT | NEWLINE IDENT non-empty map DEIDENT ))*