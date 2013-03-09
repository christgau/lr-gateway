// You may adopt your own practices by all means, but in general it is best
 // to create a single include for your project, that will include the ANTLR3 C
 // runtime header files, the generated header files (all of which are safe to include
 // multiple times) and your own project related header files. Use <> to include and
 // -I on the compile line (which vs2005 now handles, where vs2003 did not).
 //
 #include    <antlr3treeparser.h>
 #include    <configurationLexer.h>
 #include    <configurationParser.h>
 //#include    <expr_eval.h>
 
 // Main entry point for this example
 //
 int ANTLR3_CDECL
 main   (int argc, char *argv[])
 {
     // Now we declare the ANTLR related local variables we need.
     // Note that unless you are convinced you will never need thread safe
     // versions for your project, then you should always create such things
     // as instance variables for each invocation.
     // -------------------
 
     // Name of the input file. Note that we always use the abstract type pANTLR3_UINT8
     // for ASCII/8 bit strings - the runtime library guarantees that this will be
     // good on all platforms. This is a general rule - always use the ANTLR3 supplied
     // typedefs for pointers/types/etc.
     //
     pANTLR3_UINT8      fName;
 
     // The ANTLR3 character input stream, which abstracts the input source such that
     // it is easy to privide inpput from different sources such as files, or 
     // memory strings.
     //
     // For an 8Bit/latin-1/etc memory string use:
     //     input = antlr3New8BitStringInPlaceStream (stringtouse, (ANTLR3_UINT32) length, NULL);
     //
     // For a UTF16 memory string use:
     //     input = antlr3NewUTF16StringInPlaceStream (stringtouse, (ANTLR3_UINT32) length, NULL);
     //
     // For input from a file, see code below
     //
     // Note that this is essentially a pointer to a structure containing pointers to functions.
     // You can create your own input stream type (copy one of the existing ones) and override any
     // individual function by installing your own pointer after you have created the standard 
     // version.
     //
     pANTLR3_INPUT_STREAM       input;
 
     // The lexer is of course generated by ANTLR, and so the lexer type is not upper case.
     // The lexer is supplied with a pANTLR3_INPUT_STREAM from whence it consumes its
     // input and generates a token stream as output. This is the ctx (CTX macro) pointer
        // for your lexer.
     //
     pconfigurationLexer             lxr;
 
     // The token stream is produced by the ANTLR3 generated lexer. Again it is a structure based
     // API/Object, which you can customise and override methods of as you wish. a Token stream is
     // supplied to the generated parser, and you can write your own token stream and pass this in
     // if you wish.
     //
     pANTLR3_COMMON_TOKEN_STREAM        tstream;
 
     // The oris_automation parser is also generated by ANTLR and accepts a token stream as explained
     // above. The token stream can be any source in fact, so long as it implements the 
     // ANTLR3_TOKEN_SOURCE interface. In this case the parser does not return anything
     // but it can of course specify any kind of return type from the rule you invoke
     // when calling it. This is the ctx (CTX macro) pointer for your parser.
     //
     pconfigurationParser                psr;
 
     // The parser produces an AST, which is returned as a member of the return type of
     // the starting rule (any rule can start first of course). This is a generated type
     // based upon the rule we start with.
     //
//     configurationParser_expr_return     langAST;
	 configurationParser_configuration_return langAST;
 
 
     // The tree nodes are managed by a tree adaptor, which doles
     // out the nodes upon request. You can make your own tree types and adaptors
     // and override the built in versions. See runtime source for details and
     // eventually the wiki entry for the C target.
     //
     pANTLR3_COMMON_TREE_NODE_STREAM    nodes;
 
     // Finally, when the parser runs, it will produce an AST that can be traversed by the 
     // the tree parser: c.f. oris_automationDumpDecl.g3t This is the ctx (CTX macro) pointer for your
        // tree parser.
     //
     //pconfigurationParser          treePsr;
 
     // Create the input stream based upon the argument supplied to us on the command line
     // for this example, the input will always default to ./input if there is no explicit
     // argument.
     //
    if (argc < 2 || argv[1] == NULL)
    {
        fName   =(pANTLR3_UINT8)"./input"; // Note in VS2005 debug, working directory must be configured
    }
    else
    {
        fName   = (pANTLR3_UINT8)argv[1];
    }
 
     // Create the input stream using the supplied file name
     // (Use antlr38BitFileStreamNew for UTF16 input).
     //
     input  = antlr3FileStreamNew(fName, ANTLR3_ENC_UTF8);  /* antlr38BitFileStreamNew(fName);*/
 
     // The input will be created successfully, providing that there is enough
     // memory and the file exists etc
     //
     if ( input == NULL )
     {
            ANTLR3_FPRINTF(stderr, "Unable to open file %s due to malloc() failure1\n", (char *)fName);
     }
 
     // Our input stream is now open and all set to go, so we can create a new instance of our
     // lexer and set the lexer input to our input stream:
     //  (file | memory | ?) --> inputstream -> lexer --> tokenstream --> parser ( --> treeparser )?
     //
     lxr        = configurationLexerNew(input);      // CLexerNew is generated by ANTLR
 
     // Need to check for errors
     //
     if ( lxr == NULL )
     {
            ANTLR3_FPRINTF(stderr, "Unable to create the lexer due to malloc() failure1\n");
            exit(ANTLR3_ERR_NOMEM);
     }
 
     // Our lexer is in place, so we can create the token stream from it
     // NB: Nothing happens yet other than the file has been read. We are just 
     // connecting all these things together and they will be invoked when we
     // call the parser rule. ANTLR3_SIZE_HINT can be left at the default usually
     // unless you have a very large token stream/input. Each generated lexer
     // provides a token source interface, which is the second argument to the
     // token stream creator.
     // Note tha even if you implement your own token structure, it will always
     // contain a standard common token within it and this is the pointer that
     // you pass around to everything else. A common token as a pointer within
     // it that should point to your own outer token structure.
     //
     tstream = antlr3CommonTokenStreamSourceNew(ANTLR3_SIZE_HINT, TOKENSOURCE(lxr));
 
     if (tstream == NULL)
     {
        ANTLR3_FPRINTF(stderr, "Out of memory trying to allocate token stream\n");
        exit(ANTLR3_ERR_NOMEM);
     }
 
     // Finally, now that we have our lexer constructed, we can create the parser
     //
     psr        = configurationParserNew(tstream);  // CParserNew is generated by ANTLR3
 
     if (psr == NULL)
     {
        ANTLR3_FPRINTF(stderr, "Out of memory trying to allocate parser\n");
        exit(ANTLR3_ERR_NOMEM);
     }
 
     // We are all ready to go. Though that looked complicated at first glance,
     // I am sure, you will see that in fact most of the code above is dealing
     // with errors and there isn;t really that much to do (isn;t this always the
     // case in C? ;-).
     //
     // So, we now invoke the parser. All elements of ANTLR3 generated C components
     // as well as the ANTLR C runtime library itself are pseudo objects. This means
     // that they are represented as pointers to structures, which contain any
     // instance data they need, and a set of pointers to other interfaces or
     // 'methods'. Note that in general, these few pointers we have created here are
     // the only things you will ever explicitly free() as everything else is created
     // via factories, that allocate memory efficiently and free() everything they use
     // automatically when you close the parser/lexer/etc.
     //
     // Note that this means only that the methods are always called via the object
     // pointer and the first argument to any method, is a pointer to the structure itself.
     // It also has the side advantage, if you are using an IDE such as VS2005 that can do it
     // that when you type ->, you will see a list of all the methods the object supports.
     //
     //langAST = psr->expr(psr);
	 langAST = psr->configuration(psr);
 
     // If the parser ran correctly, we will have a tree to parse. In general I recommend
     // keeping your own flags as part of the error trapping, but here is how you can
     // work out if there were errors if you are using the generic error messages
     //
    if (psr->pParser->rec->state->errorCount > 0)
    {
        ANTLR3_FPRINTF(stderr, "The parser returned %d errors, tree walking aborted.\n", psr->pParser->rec->state->errorCount);
 
    }
    else
    {
    	ANTLR3_FPRINTF(stdout, "valid expressiong...dumping tree\n");
		ANTLR3_FPRINTF(stdout, "%s\n", langAST.tree->toStringTree(langAST.tree)->chars);
/*
 *  this code works
        nodes   = antlr3CommonTreeNodeStreamNewTree(langAST.tree, ANTLR3_SIZE_HINT); // sIZE HINT WILL SOON BE DEPRECATED!!
        // Tree parsers are given a common tree node stream (or your override)
        //
	
	oris_interpreter_init();

	pexpr_eval walker = expr_evalNew(nodes);
	oris_grammar_expr_value_t* result = walker->expr(walker);
	oris_expr_dump(result);

	oris_interpreter_finalize();
*/
 //       treePsr = oris_automationParserNew(nodes); 
/* 
        treePsr->expr(treePsr);
        nodes   ->free  (nodes);        nodes   = NULL;
        treePsr ->free  (treePsr);      treePsr = NULL;
 */
    }

    // We did not return anything from this parser rule, so we can finish. It only remains
    // to close down our open objects, in the reverse order we created them
    //
    psr     ->free  (psr);      psr     = NULL;
    tstream ->free  (tstream);  tstream = NULL;
    lxr     ->free  (lxr);      lxr     = NULL;
    input   ->close (input);    input   = NULL;
 
     return 0;
 }

