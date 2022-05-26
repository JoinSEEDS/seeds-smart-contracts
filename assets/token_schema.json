{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$id": "https://example.com/employee.schema.json",
    "title": "Record of employee",
    "description": "This document records the details of an employee",
    "type": "object",
    "properties": {
        "id": {
            "description": "A unique identifier for an employee",
            "type": "number"
        },
        "name": {
            "description": "name of the employee",
            "type": "string",
            "minLength":2
        },
        "age": {
            "description": "age of the employee",
            "type": "number",
            "minimum": 16
        },
        "hobbies": {
            "description": "hobbies of the employee",
            "type": "object",
            "properties": {
                "indoor": {
                    "type": "array",
                    "items": {
                        "description": "List of hobbies",
                        "type": "string"
                    },
                    "minItems": 1,
                    "uniqueItems": true
                },
                "outdoor": {
                    "type": "array",
                    "items": {
                        "description": "List of hobbies",
                        "type": "string"
                    },
                    "minItems": 1,
                    "uniqueItems": true
                }
            },
            "required": [
                "indoor",
                "outdoor"
            ]
        }
    },
    "required": [
        "id",
        "name",
        "age",
        "hobbies"
    ],
 "additionalProperties": false
}
