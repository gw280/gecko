/// Copyright (c) 2012 Ecma International.  All rights reserved. 
/// Ecma International makes this code available under the terms and conditions set
/// forth on http://hg.ecmascript.org/tests/test262/raw-file/tip/LICENSE (the 
/// "Use Terms").   Any redistribution of this code must retain the above 
/// copyright and this notice and otherwise comply with the Use Terms.
/**
 * Refer 13.1; 
 * It is a SyntaxError if any Identifier value occurs more than once within a FormalParameterList of a strict mode
 * FunctionDeclaration or FunctionExpression.
 *
 * @path ch13/13.1/13.1-34-s.js
 * @description Strict Mode - SyntaxError is thrown if a function declaration has three identical parameters with a strict mode body
 * @onlyStrict
 */


function testcase() {

        try {
            eval("var _13_1_34_fun = function (param, param, param) { 'use strict'; };")
            return false;
        } catch (e) {
            return e instanceof SyntaxError;
        }
    }
runTestCase(testcase);
